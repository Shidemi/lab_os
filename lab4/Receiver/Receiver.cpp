// Receiver.cpp
// Lab 4 - Inter-Process Communication via shared binary file (circular FIFO queue)
//
// Receiver:
//   1. Creates the binary queue file and initialises it.
//   2. Launches N Sender processes (passes the filename via command line).
//   3. Waits for all Senders to signal "ready".
//   4. Reads messages from the queue on demand (console command 'r' = read, 'q' = quit).
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <cstdlib>

#include "../include/shared.h"
#include "../include/win_error.h"
#include "../include/scoped_handle.h"
#include "../include/queue_io.h"

// --- Constants ---------------------------------------------------------------

static const int kMinCapacity   = 1;
static const int kMaxCapacity   = kMaxQueueSlots;
static const int kMinSenders    = 1;
static const int kMaxSenders    = 32;
static const char* kSenderExe   = "Sender.exe";

// --- Input helpers -----------------------------------------------------------

static int PromptInt(const std::string& prompt, int lo, int hi) {
    while (true) {
        std::cout << prompt;
        int v;
        if (std::cin >> v && v >= lo && v <= hi) return v;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cerr << "  Enter an integer between " << lo << " and " << hi << ".\n";
    }
}

static std::string PromptLine(const std::string& prompt) {
    std::cout << prompt;
    if (std::cin.peek() == '\n') std::cin.ignore();
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// --- File creation -----------------------------------------------------------

static void CreateQueueFile(const std::string& filename, int capacity) {
    std::FILE* f = std::fopen(filename.c_str(), "wb");
    if (nullptr == f) {
        throw std::runtime_error("Cannot create queue file '" + filename + "'");
    }

    QueueHeader hdr;
    hdr.head     = 0;
    hdr.tail     = 0;
    hdr.count    = 0;
    hdr.capacity = capacity;

    std::fwrite(&hdr, sizeof(QueueHeader), 1, f);

    // Write empty record slots
    MessageRecord empty;
    for (int i = 0; i < capacity; ++i) {
        std::fwrite(&empty, sizeof(MessageRecord), 1, f);
    }

    std::fclose(f);
    std::cout << "[Receiver] Queue file '" << filename
              << "' created with " << capacity << " slot(s).\n";
}

// --- Named sync objects ------------------------------------------------------

struct SyncObjects {
    ScopedHandle mutex;
    ScopedHandle semItems;  // counts available messages
    ScopedHandle semSpace;  // counts free slots
    std::vector<ScopedHandle> readyEvents; // one per Sender

    SyncObjects() = default;
    SyncObjects(const SyncObjects&) = delete;
    SyncObjects& operator=(const SyncObjects&) = delete;
};

static void CreateSyncObjects(SyncObjects& sync, int capacity, int senderCount) {
    // Mutex starts signalled (unlocked).
    sync.mutex = ScopedHandle(
        CreateMutexA(nullptr, FALSE, kMutexName)
    );
    if (NULL == sync.mutex.get()) ThrowLastError("CreateMutex failed");

    // semItems starts at 0 (queue is empty)
    sync.semItems = ScopedHandle(
        CreateSemaphoreA(nullptr, 0, capacity, kSemItemsName)
    );
    if (NULL == sync.semItems.get()) ThrowLastError("CreateSemaphore(items) failed");

    // semSpace starts at capacity (all slots free)
    sync.semSpace = ScopedHandle(
        CreateSemaphoreA(nullptr, capacity, capacity, kSemSpaceName)
    );
    if (NULL == sync.semSpace.get()) ThrowLastError("CreateSemaphore(space) failed");

    // Per-sender ready events (auto-reset, initially not signalled)
    sync.readyEvents.resize(static_cast<size_t>(senderCount));
    for (int i = 0; i < senderCount; ++i) {
        std::string name = std::string(kReadyEventPrefix) + std::to_string(i + 1);
        sync.readyEvents[static_cast<size_t>(i)] = ScopedHandle(
            CreateEventA(nullptr, FALSE, FALSE, name.c_str())
        );
        if (NULL == sync.readyEvents[static_cast<size_t>(i)].get()) {
            ThrowLastError("CreateEvent failed for " + name);
        }
    }
}

// --- Launching Senders -------------------------------------------------------

static void LaunchSenders(
    const std::string& queueFilename,
    int senderCount,
    std::vector<ScopedHandle>& processHandles
) {
    processHandles.resize(static_cast<size_t>(senderCount));

    for (int i = 0; i < senderCount; ++i) {
        // Command line: Sender.exe "<filename>" <sender_ordinal>
        std::ostringstream oss;
        oss << kSenderExe << " \"" << queueFilename << "\" " << (i + 1);
        std::string cmdLine = oss.str();

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        std::memset(&si, 0, sizeof(si));
        std::memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);

        if (!CreateProcessA(nullptr, &cmdLine[0], nullptr, nullptr,
                            FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
            ThrowLastError("CreateProcess failed for Sender " + std::to_string(i + 1));
        }

        // Close thread handle immediately; only the process handle is needed.
        CloseHandle(pi.hThread);
        processHandles[static_cast<size_t>(i)] = ScopedHandle(pi.hProcess);
        std::cout << "[Receiver] Sender " << (i + 1) << " launched (pid=" << pi.dwProcessId << ").\n";
    }
}

// --- Wait for all Senders to be ready ----------------------------------------

static void WaitForAllSendersReady(SyncObjects& sync, int senderCount) {
    std::cout << "[Receiver] Waiting for all Senders to signal ready...\n";
    for (int i = 0; i < senderCount; ++i) {
        WaitForSingleObject(sync.readyEvents[static_cast<size_t>(i)].get(), INFINITE);
        std::cout << "[Receiver] Sender " << (i + 1) << " is ready.\n";
    }
    std::cout << "[Receiver] All Senders ready. You can start reading messages.\n";
}

// --- Main read loop ----------------------------------------------------------

static void RunReadLoop(const std::string& queueFilename, SyncObjects& sync) {
    std::cout << "\nCommands:  r = read next message   q = quit\n";

    while (true) {
        std::cout << "> ";
        std::string cmd;
        std::cin >> cmd;

        if (cmd == "q" || cmd == "Q") {
            std::cout << "[Receiver] Quitting.\n";
            break;
        }

        if (cmd != "r" && cmd != "R") {
            std::cerr << "  Unknown command. Use 'r' or 'q'.\n";
            continue;
        }

        // Block until a message is available
        std::cout << "[Receiver] Waiting for a message...\n";
        WaitForSingleObject(sync.semItems.get(), INFINITE);

        // Lock the file, dequeue the message, unlock
        WaitForSingleObject(sync.mutex.get(), INFINITE);
        std::FILE* f = std::fopen(queueFilename.c_str(), "r+b");
        if (nullptr == f) {
            ReleaseMutex(sync.mutex.get());
            throw std::runtime_error("Cannot open queue file for reading");
        }

        MessageRecord rec;
        try {
            rec = DequeueMessage(f);
        } catch (...) {
            std::fclose(f);
            ReleaseMutex(sync.mutex.get());
            // Release the semItems token we consumed because something went wrong.
            ReleaseSemaphore(sync.semItems.get(), 1, nullptr);
            throw;
        }

        std::fclose(f);
        ReleaseMutex(sync.mutex.get());

        // A slot is now free; wake a blocked Sender if any.
        ReleaseSemaphore(sync.semSpace.get(), 1, nullptr);

        std::cout << "[Receiver] Message from Sender " << rec.senderId
                  << ": \"" << rec.text << "\"\n";
    }
}

// --- Entry point -------------------------------------------------------------

int main() {
    try {
        std::cout << "=== Lab 4: IPC via Shared Binary File (Receiver) ===\n\n";

        std::string filename = PromptLine("Enter queue file name: ");
        if (filename.empty()) throw std::invalid_argument("File name cannot be empty");

        int capacity = PromptInt(
            "Enter queue capacity (max records): ",
            kMinCapacity, kMaxCapacity
        );

        int senderCount = PromptInt(
            "Enter number of Sender processes: ",
            kMinSenders, kMaxSenders
        );

        // 1. Create the queue file
        CreateQueueFile(filename, capacity);

        // 2. Create named synchronization objects
        SyncObjects sync;
        CreateSyncObjects(sync, capacity, senderCount);

        // 3. Launch Sender processes
        std::vector<ScopedHandle> processHandles;
        LaunchSenders(filename, senderCount, processHandles);

        // 4. Wait for all Senders to be ready
        WaitForAllSendersReady(sync, senderCount);

        // 5. Read messages on demand
        RunReadLoop(filename, sync);

        std::cout << "[Receiver] Done.\n";

        // ScopedHandles in processHandles + sync members released in reverse order
        // by their destructors when they go out of scope

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
