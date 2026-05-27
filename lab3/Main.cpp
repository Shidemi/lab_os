// Main.cpp
// Lab 3 - Thread Synchronization: Critical Sections and Events
//
// Threads:
//   main   - orchestrator: starts markers, handles deadlock rounds, terminates one at a time
//   marker - writes its index into free array cells; signals "stuck" when no free cell found

#include <windows.h>
#include <iostream>
#include <vector>
#include <limits>
#include <stdexcept>
#include <string>
#include <cstdlib>   // srand, rand
#include <cstring>   // memset

#include "win_error.h"
#include "scoped_handle.h"
#include "program_state.h"

// --- Constants ---------------------------------------------------------------

static const DWORD kSleepBeforeWriteMs = 5;   // Sleep before writing to array cell
static const DWORD kSleepAfterWriteMs  = 5;   // Sleep after writing to array cell
static const int   kMinArraySize       = 2;
static const int   kMaxArraySize       = 100000;
static const int   kMinMarkerCount     = 1;
static const int   kMaxMarkerCount     = 64;   // WaitForMultipleObjects limit is 64

// --- Input helpers -----------------------------------------------------------

static int PromptInt(const std::string& prompt, int minVal, int maxVal) {
    while (true) {
        std::cout << prompt;
        int value;
        if (std::cin >> value && value >= minVal && value <= maxVal) {
            return value;
        }
        std::cin.clear();
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        std::cerr << "  Invalid input. Enter an integer between "
                  << minVal << " and " << maxVal << ".\n";
    }
}

// --- Array helpers -----------------------------------------------------------

static void PrintArray(const std::vector<int>& array) {
    std::cout << "Array: [ ";
    for (const int val : array) {
        std::cout << val << " ";
    }
    std::cout << "]\n";
}

// Zeroes out all cells marked by the given marker index
static void ClearMarkerCells(int* array, int arraySize, int markerIndex) {
    for (int i = 0; i < arraySize; ++i) {
        if (array[i] == markerIndex) {
            array[i] = 0;
        }
    }
}

// Counts how many cells are marked by the given marker index
static int CountMarkedCells(const int* array, int arraySize, int markerIndex) {
    int count = 0;
    for (int i = 0; i < arraySize; ++i) {
        if (array[i] == markerIndex) {
            ++count;
        }
    }
    return count;
}

// --- Marker thread -----------------------------------------------------------

DWORD WINAPI ThreadMarker(LPVOID param) {
    // Take ownership of params (allocated by main, freed here)
    MarkerParams* p = reinterpret_cast<MarkerParams*>(param);

    const int markerIndex = p->markerIndex;
    int* const array      = p->array;
    const int arraySize   = p->arraySize;
    CRITICAL_SECTION* cs  = p->cs;
    MarkerEvents events   = p->events;

    delete p; // Free the heap-allocated params struct

    // Step 1: wait for main's "go" signal
    WaitForSingleObject(events.startEvent, INFINITE);

    // Step 2: seed RNG with this thread's ordinal
    srand(static_cast<unsigned int>(markerIndex));

    // Step 3: work loop
    while (true) {
        // 3.1 & 3.2: generate random index
        int index = rand() % arraySize;

        // 3.3: check if cell is free
        EnterCriticalSection(cs);
        bool cellIsFree = (array[index] == 0);
        if (cellIsFree) {
            // 3.3.1 sleep before write (inside CS to preserve the "free" guarantee)
            Sleep(kSleepBeforeWriteMs);
            // 3.3.2: mark the cell
            array[index] = markerIndex;
            LeaveCriticalSection(cs);
            // 3.3.3: sleep after write
            Sleep(kSleepAfterWriteMs);
            // 3.3.4: continue loop
            continue;
        }
        LeaveCriticalSection(cs);

        // 3.4: cell was not free; report and signal stuck.
        int marked = CountMarkedCells(array, arraySize, markerIndex);
        std::cout << "[marker " << markerIndex << "] stuck at index=" << index
                  << ", marked=" << marked << " cell(s)\n";

        // 3.4.2: signal main that this thread is stuck
        SetEvent(events.stuckEvent);

        // 3.4.3: wait for continue or stop signal
        HANDLE waitHandles[2] = { events.continueEvent, events.stopEvent };
        DWORD result = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (WAIT_OBJECT_0 == result) {
            // Received "continue"; resume loop.
            continue;
        }

        // Received "stop" (or error treated as stop)
        // Step 4.1: clear all cells this marker wrote
        EnterCriticalSection(cs);
        ClearMarkerCells(array, arraySize, markerIndex);
        LeaveCriticalSection(cs);

        std::cout << "[marker " << markerIndex << "] terminated, cells cleared.\n";
        return EXIT_SUCCESS;
    }
}

// --- Event creation helpers --------------------------------------------------

static HANDLE CreateAutoResetEvent(const std::string& name) {
    HANDLE h = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (NULL == h) ThrowLastError("CreateEvent (auto-reset) failed for " + name);
    return h;
}

static HANDLE CreateManualResetEvent(const std::string& name) {
    HANDLE h = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (NULL == h) ThrowLastError("CreateEvent (manual-reset) failed for " + name);
    return h;
}

// --- Program state setup and teardown ----------------------------------------

static void InitializeProgramState(ProgramState& state) {
    InitializeCriticalSection(&state.cs);

    // Single broadcast event: all markers start simultaneously
    state.startEvent = CreateManualResetEvent("startEvent");

    state.markerEvents.resize(static_cast<size_t>(state.markerCount));
    for (int i = 0; i < state.markerCount; ++i) {
        MarkerEvents& me = state.markerEvents[static_cast<size_t>(i)];
        me.startEvent    = state.startEvent; // shared handle
        me.stuckEvent    = CreateAutoResetEvent("stuckEvent_"    + std::to_string(i + 1));
        me.continueEvent = CreateAutoResetEvent("continueEvent_" + std::to_string(i + 1));
        me.stopEvent     = CreateManualResetEvent("stopEvent_"   + std::to_string(i + 1));
    }
}

static void DestroyProgramState(ProgramState& state) {
    // Release events in reverse acquisition order
    for (int i = state.markerCount - 1; i >= 0; --i) {
        MarkerEvents& me = state.markerEvents[static_cast<size_t>(i)];
        CloseHandle(me.stopEvent);
        CloseHandle(me.continueEvent);
        CloseHandle(me.stuckEvent);
        // me.startEvent is the shared startEvent, closed once below.
    }
    CloseHandle(state.startEvent);
    DeleteCriticalSection(&state.cs);
}

// --- Thread launching --------------------------------------------------------

static void LaunchMarkerThreads(ProgramState& state) {
    state.threadHandles.resize(static_cast<size_t>(state.markerCount), nullptr);

    for (int i = 0; i < state.markerCount; ++i) {
        // Heap-allocate params; ownership transferred to and freed inside the thread
        MarkerParams* params = new MarkerParams();
        params->markerIndex  = i + 1;
        params->array        = state.array.data();
        params->arraySize    = static_cast<int>(state.array.size());
        params->cs           = &state.cs;
        params->events       = state.markerEvents[static_cast<size_t>(i)];

        DWORD threadId = 0;
        HANDLE h = CreateThread(nullptr, 0, ThreadMarker, params, 0, &threadId);
        if (NULL == h) {
            delete params;
            ThrowLastError("CreateThread failed for marker " + std::to_string(i + 1));
        }
        state.threadHandles[static_cast<size_t>(i)] = h;
        std::cout << "[main] Marker thread " << (i + 1) << " started (id=" << threadId << ").\n";
    }
}

// --- Main synchronization loop -----------------------------------------------

// Waits until all still-running marker threads have signalled "stuck".
// Returns the set of indices (0-based) of threads that are still alive.
static std::vector<int> WaitForAllStuck(ProgramState& state, const std::vector<bool>& alive) {
    std::vector<int> stuckIndices;
    for (int i = 0; i < state.markerCount; ++i) {
        if (alive[static_cast<size_t>(i)]) {
            WaitForSingleObject(
                state.markerEvents[static_cast<size_t>(i)].stuckEvent,
                INFINITE
            );
            stuckIndices.push_back(i);
        }
    }
    return stuckIndices;
}

static void RunMainLoop(ProgramState& state) {
    std::vector<bool> alive(static_cast<size_t>(state.markerCount), true);
    int aliveCount = state.markerCount;

    while (aliveCount > 0) {
        // 6.1: wait until all running threads are stuck
        std::cout << "\n[main] Waiting for all marker threads to report stuck...\n";
        WaitForAllStuck(state, alive);

        // 6.2: print array
        PrintArray(state.array);

        // 6.3: ask which thread to terminate
        std::cout << "Enter marker thread number to terminate (1-" << state.markerCount << "): ";
        int chosenIndex = -1;
        while (true) {
            int num = PromptInt("", 1, state.markerCount);
            int idx = num - 1;
            if (alive[static_cast<size_t>(idx)]) {
                chosenIndex = idx;
                break;
            }
            std::cerr << "  Marker " << num << " is already terminated. Choose another.\n";
        }

        // 6.4: signal chosen thread to stop
        SetEvent(state.markerEvents[static_cast<size_t>(chosenIndex)].stopEvent);

        // 6.5: wait for that thread to finish
        WaitForSingleObject(state.threadHandles[static_cast<size_t>(chosenIndex)], INFINITE);
        CloseHandle(state.threadHandles[static_cast<size_t>(chosenIndex)]);
        state.threadHandles[static_cast<size_t>(chosenIndex)] = nullptr;
        alive[static_cast<size_t>(chosenIndex)] = false;
        --aliveCount;

        std::cout << "[main] Marker " << (chosenIndex + 1) << " terminated.\n";

        // 6.6: print array after termination
        PrintArray(state.array);

        // 6.7: signal remaining threads to continue
        if (aliveCount > 0) {
            std::cout << "[main] Signalling remaining threads to continue...\n";
            for (int i = 0; i < state.markerCount; ++i) {
                if (alive[static_cast<size_t>(i)]) {
                    SetEvent(state.markerEvents[static_cast<size_t>(i)].continueEvent);
                }
            }
        }
    }
}

// --- Entry point -------------------------------------------------------------

int main() {
    ProgramState state;
    state.markerCount = 0;
    state.startEvent  = nullptr;

    try {
        std::cout << "=== Lab 3: Thread Synchronization (Critical Sections & Events) ===\n\n";

        // Step 1 & 2: allocate and zero-initialize the array
        int arraySize = PromptInt("Enter array size: ", kMinArraySize, kMaxArraySize);
        state.array.assign(static_cast<size_t>(arraySize), 0);

        // Step 3: ask for number of marker threads
        state.markerCount = PromptInt(
            "Enter number of marker threads: ",
            kMinMarkerCount,
            kMaxMarkerCount
        );

        // Initialize critical section and all events
        InitializeProgramState(state);

        // Step 4: launch all marker threads (they block on startEvent)
        LaunchMarkerThreads(state);

        // Step 5: broadcast "go" to all marker threads simultaneously
        std::cout << "\n[main] Signalling all markers to start...\n";
        SetEvent(state.startEvent);

        // Steps 6 & 7: run the termination loop
        RunMainLoop(state);

        std::cout << "\n[main] All marker threads finished. Program complete.\n";

    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
        DestroyProgramState(state);
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        DestroyProgramState(state);
        return EXIT_FAILURE;
    }

    // Release all resources in reverse acquisition order
    DestroyProgramState(state);
    return EXIT_SUCCESS;
}
