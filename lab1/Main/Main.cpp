// Main.cpp
// Orchestrator program: launches Creator and Reporter as child processes
// using the Windows API, waits for their completion, and displays results.
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <iomanip>
#include <limits>
#include <cstring>
#include <cstdlib>

#include "../include/employee.h"
#include "../include/win_error.h"

// --- Constants ---------------------------------------------------------------

static const DWORD  kWaitTimeoutMs      = INFINITE;
static const int    kMinRecordCount     = 1;
static const int    kMaxRecordCount     = 10000;
static const char*  kCreatorExecutable  = "Creator.exe";
static const char*  kReporterExecutable = "Reporter.exe";

// --- RAII handle wrapper -----------------------------------------------------

// Ensures CloseHandle is always called in reverse order of acquisition
class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle) : handle_(handle) {}

    ~ScopedHandle() {
        if (NULL != handle_ && INVALID_HANDLE_VALUE != handle_) {
            CloseHandle(handle_);
        }
    }

    HANDLE get() const { return handle_; }

    // Non-copyable
    ScopedHandle(const ScopedHandle&)            = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

private:
    HANDLE handle_;
};

// --- Input helpers -----------------------------------------------------------

// Prompts the user and reads a trimmed line
static std::string PromptLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    // Clear any leftover newline in the buffer
    if (std::cin.peek() == '\n') {
        std::cin.ignore();
    }
    if (!std::getline(std::cin, line)) {
        throw std::runtime_error("Failed to read input");
    }
    return line;
}

// Prompts the user and reads a validated integer
static int PromptInt(const std::string& prompt, int minVal, int maxVal) {
    while (true) {
        std::cout << prompt;
        int value;
        if (std::cin >> value) {
            if (value >= minVal && value <= maxVal) {
                return value;
            }
            std::cerr << "  Value must be between " << minVal << " and " << maxVal << ". Try again.\n";
        } else {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cerr << "  Invalid input. Please enter an integer.\n";
        }
    }
}

// Prompts the user and reads a non-negative double
static double PromptDouble(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        double value;
        if (std::cin >> value && value >= 0.0) {
            return value;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cerr << "  Invalid input. Please enter a non-negative number.\n";
    }
}

// --- Process launching -------------------------------------------------------

// Builds a mutable command-line string (CreateProcess requires non-const buffer)
static std::string BuildCommandLine(
    const std::string& executable,
    const std::string& arg1,
    const std::string& arg2
) {
    std::ostringstream oss;
    oss << executable << " \"" << arg1 << "\" \"" << arg2 << "\"";
    return oss.str();
}

static std::string BuildCommandLine(
    const std::string& executable,
    const std::string& arg1,
    const std::string& arg2,
    const std::string& arg3
) {
    std::ostringstream oss;
    oss << executable << " \"" << arg1 << "\" \"" << arg2 << "\" \"" << arg3 << "\"";
    return oss.str();
}

// Launches a child process and waits for it to finish.
// Resources are released in reverse acquisition order via ScopedHandle (RAII).
static void LaunchAndWait(const std::string& commandLine) {
    std::cout << "Launching: " << commandLine << "\n";

    // CreateProcess requires a mutable buffer for lpCommandLine
    std::string mutableCmdLine = commandLine;

    STARTUPINFOA  startupInfo;
    PROCESS_INFORMATION processInfo;
    std::memset(&startupInfo,  0, sizeof(STARTUPINFOA));
    std::memset(&processInfo,  0, sizeof(PROCESS_INFORMATION));
    startupInfo.cb = sizeof(STARTUPINFOA);

    BOOL created = CreateProcessA(
        nullptr,                         // Application name (use command line)
        &mutableCmdLine[0],              // Mutable command line
        nullptr,                         // Process security attributes
        nullptr,                         // Thread security attributes
        FALSE,                           // Do not inherit handles
        0,                               // Creation flags
        nullptr,                         // Inherit environment
        nullptr,                         // Inherit current directory
        &startupInfo,
        &processInfo
    );

    if (FALSE == created) {
        ThrowLastError("CreateProcess failed for: " + commandLine);
    }

    // Acquire handles; released in reverse order on scope exit.
    ScopedHandle threadHandle(processInfo.hThread);
    ScopedHandle processHandle(processInfo.hProcess);

    DWORD waitResult = WaitForSingleObject(processHandle.get(), kWaitTimeoutMs);
    if (WAIT_OBJECT_0 != waitResult) {
        ThrowLastError("WaitForSingleObject failed");
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processHandle.get(), &exitCode)) {
        ThrowLastError("GetExitCodeProcess failed");
    }
    if (EXIT_SUCCESS != exitCode) {
        throw std::runtime_error(
            "Child process exited with error code: " + std::to_string(exitCode)
        );
    }

    std::cout << "Process completed successfully.\n";
    // ScopedHandle destructors: ~processHandle, then ~threadHandle (reverse order)
}

// --- File display helpers ----------------------------------------------------

// Displays the contents of the binary employee file to stdout
static void DisplayBinaryFile(const std::string& filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile.is_open()) {
        throw std::runtime_error("Cannot open binary file '" + filename + "' for display");
    }

    std::cout << "\n=== Contents of binary file: " << filename << " ===\n";
    std::cout << std::left
              << std::setw(8)  << "ID"
              << std::setw(12) << "Name"
              << std::setw(10) << "Hours"
              << "\n";
    std::cout << std::string(30, '-') << "\n";

    Employee emp;
    int count = 0;
    while (inFile.read(reinterpret_cast<char*>(&emp), sizeof(Employee))) {
        std::cout << std::left
                  << std::setw(8)  << emp.num
                  << std::setw(12) << emp.name
                  << std::setw(10) << std::fixed << std::setprecision(2) << emp.hours
                  << "\n";
        ++count;
    }

    if (0 == count) {
        std::cout << "(no records found)\n";
    }
    std::cout << "=== Total: " << count << " record(s) ===\n\n";
}

// Displays the contents of a text report file to stdout
static void DisplayTextFile(const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        throw std::runtime_error("Cannot open report file '" + filename + "' for display");
    }

    std::cout << "\n=== Report: " << filename << " ===\n";
    std::string line;
    while (std::getline(inFile, line)) {
        std::cout << line << "\n";
    }
    std::cout << "=== End of report ===\n\n";
}

// --- Main flow ---------------------------------------------------------------

int main() {
    try {
        // Step 1: request binary file name and record count
        std::cout << "=== Lab 1: Process Creation (Windows API) ===\n\n";
        std::string binaryFilename = PromptLine("Enter binary file name: ");
        if (binaryFilename.empty()) {
            throw std::invalid_argument("Binary file name cannot be empty");
        }

        int recordCount = PromptInt(
            "Enter number of records: ",
            kMinRecordCount,
            kMaxRecordCount
        );

        // Step 2 & 3: launch Creator and wait for it
        std::string creatorCmd = BuildCommandLine(
            kCreatorExecutable,
            binaryFilename,
            std::to_string(recordCount)
        );
        LaunchAndWait(creatorCmd);

        // Step 4: display the binary file content
        DisplayBinaryFile(binaryFilename);

        // Step 5: request report file name and hourly rate
        std::string reportFilename = PromptLine("Enter report file name: ");
        if (reportFilename.empty()) {
            throw std::invalid_argument("Report file name cannot be empty");
        }

        double hourlyRate = PromptDouble("Enter hourly pay rate: ");

        // Step 6 & 7: launch Reporter and wait for it
        std::ostringstream rateStr;
        rateStr << std::fixed << std::setprecision(6) << hourlyRate;

        std::string reporterCmd = BuildCommandLine(
            kReporterExecutable,
            binaryFilename,
            reportFilename,
            rateStr.str()
        );
        LaunchAndWait(reporterCmd);

        // Step 8: display the report
        DisplayTextFile(reportFilename);

        // Step 9: done
        std::cout << "Main program finished successfully.\n";

    } catch (const std::invalid_argument& e) {
        std::cerr << "Input error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
