// Main.cpp
// Lab 2 - Thread Creation (Windows API)
//
// Three threads:
//   main     - creates array, launches min_max and average, waits, then applies results
//   min_max  - finds min and max, sleeps 7 ms after each comparison
//   average  - computes arithmetic mean, sleeps 12 ms after each addition
#define NOMINMAX

#include <windows.h>
#include <iostream>
#include <vector>
#include <limits>
#include <stdexcept>
#include <string>
#include <cmath>
#include <cstdlib>
#include "shared_data.h"
#include "win_error.h"

// --- Constants ---------------------------------------------------------------

static const DWORD kSleepMinMaxMs  = 7;   // Sleep interval in min_max thread (ms)
static const DWORD kSleepAverageMs = 12;  // Sleep interval in average thread (ms)
static const int   kMinArraySize   = 1;
static const int   kMaxArraySize   = 100000;

// --- RAII handle wrapper -----------------------------------------------------

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle) : handle_(handle) {}

    ~ScopedHandle() {
        if (NULL != handle_ && INVALID_HANDLE_VALUE != handle_) {
            CloseHandle(handle_);
        }
    }

    HANDLE get() const { return handle_; }

    ScopedHandle(const ScopedHandle&)            = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

private:
    HANDLE handle_;
};

// --- Thread functions --------------------------------------------------------

// Thread: finds minimum and maximum elements.
// Sleeps kSleepMinMaxMs ms after each comparison.
DWORD WINAPI ThreadMinMax(LPVOID param) {
    SharedData* data = reinterpret_cast<SharedData*>(param);

    int minVal = data->array[0];
    int maxVal = data->array[0];

    for (size_t i = 1; i < data->array.size(); ++i) {
        if (data->array[i] < minVal) {
            minVal = data->array[i];
        }
        Sleep(kSleepMinMaxMs);

        if (data->array[i] > maxVal) {
            maxVal = data->array[i];
        }
        Sleep(kSleepMinMaxMs);
    }

    data->minValue = minVal;
    data->maxValue = maxVal;

    std::cout << "[min_max]  min = " << minVal << ", max = " << maxVal << "\n";
    return EXIT_SUCCESS;
}

// Thread: computes arithmetic mean.
// Sleeps kSleepAverageMs ms after each addition.
DWORD WINAPI ThreadAverage(LPVOID param) {
    SharedData* data = reinterpret_cast<SharedData*>(param);

    long long sum = 0;
    for (const int value : data->array) {
        sum += value;
        Sleep(kSleepAverageMs);
    }

    data->average = static_cast<double>(sum) / static_cast<double>(data->array.size());

    std::cout << "[average]  mean = " << data->average << "\n";
    return EXIT_SUCCESS;
}

// --- Input helpers -----------------------------------------------------------

static int PromptInt(const std::string& prompt, int minVal, int maxVal) {
    while (true) {
        std::cout << prompt;
        int value;
        if (std::cin >> value && value >= minVal && value <= maxVal) {
            return value;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cerr << "  Invalid input. Enter an integer between "
                  << minVal << " and " << maxVal << ".\n";
    }
}

// --- Array helpers -----------------------------------------------------------

static std::vector<int> ReadArrayFromConsole(int size) {
    std::vector<int> arr;
    arr.reserve(static_cast<size_t>(size));

    std::cout << "Enter " << size << " integer(s):\n";
    for (int i = 0; i < size; ++i) {
        std::cout << "  [" << i << "]: ";
        int value;
        while (!(std::cin >> value)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cerr << "  Invalid input. Enter an integer.\n";
            std::cout << "  [" << i << "]: ";
        }
        arr.push_back(value);
    }
    return arr;
}

static void PrintArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << ": [ ";
    for (const int value : arr) {
        std::cout << value << " ";
    }
    std::cout << "]\n";
}

// Replaces all occurrences of minValue and maxValue with the average value
static void ReplaceMinMaxWithAverage(std::vector<int>& arr, int minVal, int maxVal, double avg) {
    int roundedAvg = static_cast<int>(std::round(avg));
    for (int& element : arr) {
        if (element == minVal || element == maxVal) {
            element = roundedAvg;
        }
    }
}

// --- Thread launching --------------------------------------------------------

static void LaunchThreadAndWait(
    LPTHREAD_START_ROUTINE threadFunc,
    LPVOID param,
    const std::string& threadName
) {
    DWORD threadId = 0;
    HANDLE rawHandle = CreateThread(
        nullptr,       // Default security attributes
        0,             // Default stack size
        threadFunc,
        param,
        0,             // Run immediately
        &threadId
    );

    if (NULL == rawHandle) {
        ThrowLastError("CreateThread failed for thread '" + threadName + "'");
    }

    ScopedHandle threadHandle(rawHandle);

    std::cout << "[main] Thread '" << threadName << "' started (id=" << threadId << ").\n";

    DWORD waitResult = WaitForSingleObject(threadHandle.get(), INFINITE);
    if (WAIT_OBJECT_0 != waitResult) {
        ThrowLastError("WaitForSingleObject failed for thread '" + threadName + "'");
    }

    DWORD exitCode = 0;
    if (!GetExitCodeThread(threadHandle.get(), &exitCode)) {
        ThrowLastError("GetExitCodeThread failed for thread '" + threadName + "'");
    }
    if (EXIT_SUCCESS != exitCode) {
        throw std::runtime_error(
            "Thread '" + threadName + "' exited with error code: " + std::to_string(exitCode)
        );
    }
}

// Launches both threads simultaneously and waits for both to finish
static void LaunchBothThreadsAndWait(SharedData& data) {
    DWORD threadIdMinMax  = 0;
    DWORD threadIdAverage = 0;

    HANDLE rawMinMax = CreateThread(nullptr, 0, ThreadMinMax,  &data, 0, &threadIdMinMax);
    if (NULL == rawMinMax) {
        ThrowLastError("CreateThread failed for thread 'min_max'");
    }
    ScopedHandle hMinMax(rawMinMax);

    HANDLE rawAverage = CreateThread(nullptr, 0, ThreadAverage, &data, 0, &threadIdAverage);
    if (NULL == rawAverage) {
        // hMinMax destructor will CloseHandle on scope exit
        ThrowLastError("CreateThread failed for thread 'average'");
    }
    ScopedHandle hAverage(rawAverage);

    std::cout << "[main] Thread 'min_max' started  (id=" << threadIdMinMax  << ").\n";
    std::cout << "[main] Thread 'average' started  (id=" << threadIdAverage << ").\n";

    DWORD waitMinMax = WaitForSingleObject(hMinMax.get(), INFINITE);
    if (waitMinMax != WAIT_OBJECT_0) {
        ThrowLastError("WaitForSingleObject failed for thread 'min_max'");
    }

    DWORD waitAverage = WaitForSingleObject(hAverage.get(), INFINITE);
    if (waitAverage != WAIT_OBJECT_0) {
        ThrowLastError("WaitForSingleObject failed for thread 'average'");
    }

    std::cout << "[main] Both threads finished.\n";

    // ScopedHandle destructors release handles in reverse order: ~hAverage, ~hMinMax
}

// --- Entry point -------------------------------------------------------------

int main() {
    try {
        std::cout << "=== Lab 2: Thread Creation (Windows API) ===\n\n";

        // Step 1: create array
        int size = PromptInt(
            "Enter array size: ",
            kMinArraySize,
            kMaxArraySize
        );

        SharedData data;
        data.array    = ReadArrayFromConsole(size);
        data.minValue = 0;
        data.maxValue = 0;
        data.average  = 0.0;

        PrintArray(data.array, "\nInitial array");

        // Steps 2 & 3: launch min_max and average threads, wait for both
        std::cout << "\n[main] Launching threads...\n";
        LaunchBothThreadsAndWait(data);

        // Step 4: replace min/max with average, print results
        std::cout << "\n[main] Replacing min (" << data.minValue
                  << ") and max (" << data.maxValue
                  << ") with average (" << data.average << ")...\n";

        ReplaceMinMaxWithAverage(data.array, data.minValue, data.maxValue, data.average);

        PrintArray(data.array, "Result array");

        // Step 5: done
        std::cout << "\n[main] Program finished successfully.\n";

    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
