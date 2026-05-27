// Client.cpp
// Lab 5 - Named Pipes: File Access Client
//
// Client:
//   1. Connects to the server pipe.
//   2. Loop: user selects Read / Modify / Quit.
//      Read:   sends READ, receives record, displays it, sends RELEASE_READ.
//      Modify: sends LOCK_WRITE, receives record, edits it, sends WRITE,
//              then sends RELEASE_WRITE.
//      Quit:   sends QUIT and exits.
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include "../include/protocol.h"
#include "../include/utils.h"

// --- Constants ---------------------------------------------------------------

static const DWORD kConnectRetries  = 10;
static const DWORD kConnectDelayMs  = 500;

// --- Pipe helpers ------------------------------------------------------------

static bool WriteExact(HANDLE pipe, const void* buf, DWORD bytes) {
    DWORD written = 0;
    return WriteFile(pipe, buf, bytes, &written, nullptr) && written == bytes;
}

static bool ReadExact(HANDLE pipe, void* buf, DWORD bytes) {
    DWORD total = 0;
    while (total < bytes) {
        DWORD read = 0;
        if (!ReadFile(pipe, static_cast<char*>(buf) + total, bytes - total, &read, nullptr) || read == 0) {
            return false;
        }
        total += read;
    }
    return true;
}

static bool SendRequest(HANDLE pipe, const Request& req) {
    return WriteExact(pipe, &req, sizeof(Request));
}

static bool RecvResponse(HANDLE pipe, Response& resp) {
    return ReadExact(pipe, &resp, sizeof(Response));
}

// --- Display helpers ---------------------------------------------------------

static void PrintEmployee(const Employee& emp) {
    std::cout << "  ID   : " << emp.num    << "\n"
              << "  Name : " << emp.name   << "\n"
              << "  Hours: " << std::fixed << std::setprecision(2) << emp.hours << "\n";
}

// --- Input helpers -----------------------------------------------------------

static int PromptInt(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        int v;
        if (std::cin >> v) return v;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cerr << "  Invalid input. Enter an integer.\n";
    }
}

static double PromptDouble(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        double v;
        if (std::cin >> v && v >= 0.0) return v;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cerr << "  Invalid input. Enter a non-negative number.\n";
    }
}

// --- Operations --------------------------------------------------------------

// 2.4 Read a record from the server
static void DoRead(HANDLE pipe) {
    int id = PromptInt("  Enter employee ID to read: ");

    // Send READ request
    Request req;
    req.op         = Operation::READ;
    req.employeeId = id;
    if (!SendRequest(pipe, req)) {
        throw std::runtime_error("Failed to send READ request");
    }

    // Receive record
    Response resp;
    if (!RecvResponse(pipe, resp)) {
        throw std::runtime_error("Failed to receive READ response");
    }

    if (resp.status != Status::OK) {
        std::cerr << "  Server error: " << resp.message << "\n";
        return;
    }

    std::cout << "  Record received (read lock held):\n";
    PrintEmployee(resp.record);

    std::cout << "  Press Enter to release read lock...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Send RELEASE_READ
    req.op = Operation::RELEASE_READ;
    if (!SendRequest(pipe, req)) {
        throw std::runtime_error("Failed to send RELEASE_READ");
    }
    Response releaseResp;
    RecvResponse(pipe, releaseResp); // consume server ack
    std::cout << "  Read lock released.\n";
}

// 2.3 Modify a record
static void DoModify(HANDLE pipe) {
    int id = PromptInt("  Enter employee ID to modify: ");

    // Send LOCK_WRITE request (server blocks until exclusive lock acquired)
    Request req;
    req.op         = Operation::LOCK_WRITE;
    req.employeeId = id;
    std::cout << "  Waiting for exclusive lock (server may block if others hold the record)...\n";
    if (!SendRequest(pipe, req)) {
        throw std::runtime_error("Failed to send LOCK_WRITE request");
    }

    Response resp;
    if (!RecvResponse(pipe, resp)) {
        throw std::runtime_error("Failed to receive LOCK_WRITE response");
    }
    if (resp.status != Status::OK) {
        std::cerr << "  Server error: " << resp.message << "\n";
        return;
    }

    std::cout << "  Record locked for editing:\n";
    PrintEmployee(resp.record);

    // Let user edit
    Employee modified = resp.record;

    std::cout << "  New name (leave blank to keep '" << modified.name << "'): ";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string newName;
    std::getline(std::cin, newName);
    if (!newName.empty()) {
        std::strncpy(modified.name, newName.c_str(), sizeof(modified.name) - 1);
        modified.name[sizeof(modified.name) - 1] = '\0';
    }

    modified.hours = PromptDouble("  New hours (current " +
                                   std::to_string(modified.hours) + "): ");

    // Send WRITE with modified record
    req.op     = Operation::WRITE;
    req.record = modified;
    if (!SendRequest(pipe, req)) {
        throw std::runtime_error("Failed to send WRITE request");
    }
    Response writeResp;
    if (!RecvResponse(pipe, writeResp)) {
        throw std::runtime_error("Failed to receive WRITE response");
    }
    if (writeResp.status == Status::OK) {
        std::cout << "  Record updated on server.\n";
    } else {
        std::cerr << "  Write failed: " << writeResp.message << "\n";
    }

    std::cout << "  Press Enter to release write lock...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Send RELEASE_WRITE
    req.op = Operation::RELEASE_WRITE;
    if (!SendRequest(pipe, req)) {
        throw std::runtime_error("Failed to send RELEASE_WRITE");
    }
    Response relResp;
    RecvResponse(pipe, relResp);
    std::cout << "  Write lock released.\n";
}

// --- Entry point -------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: Client.exe <client_ordinal> <pipe_name>\n";
        return EXIT_FAILURE;
    }

    int         clientOrdinal = std::stoi(argv[1]);
    std::string pipeName      = argv[2];

    std::cout << "=== Lab 5: Named Pipes Client " << clientOrdinal << " ===\n";
    std::cout << "    Connecting to: " << pipeName << "\n\n";

    try {
        // Connect to the named pipe (retry loop in case server hasn't called ConnectNamedPipe yet)
        HANDLE pipe = INVALID_HANDLE_VALUE;
        for (DWORD attempt = 0; attempt < kConnectRetries; ++attempt) {
            pipe = CreateFileA(
                pipeName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr,
                OPEN_EXISTING,
                0, nullptr
            );
            if (INVALID_HANDLE_VALUE != pipe) break;

            DWORD err = GetLastError();
            if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND) {
                ThrowLastError("CreateFile (pipe) failed");
            }
            std::cout << "  Pipe not ready yet, retrying...\n";
            Sleep(kConnectDelayMs);
        }
        if (INVALID_HANDLE_VALUE == pipe) {
            throw std::runtime_error("Could not connect to pipe '" + pipeName + "' after retries");
        }

        ScopedHandle pipeHandle(pipe);
        std::cout << "[Client " << clientOrdinal << "] Connected to server.\n\n";

        // Main command loop
        while (true) {
            std::cout << "Commands:  r = read   m = modify   q = quit\n";
            std::cout << "[C" << clientOrdinal << "]> ";
            std::string cmd;
            std::cin >> cmd;

            if (cmd == "q" || cmd == "Q") {
                Request req;
                req.op = Operation::QUIT;
                SendRequest(pipeHandle.get(), req);
                std::cout << "[Client " << clientOrdinal << "] Quit sent.\n";
                break;
            } else if (cmd == "r" || cmd == "R") {
                DoRead(pipeHandle.get());
            } else if (cmd == "m" || cmd == "M") {
                DoModify(pipeHandle.get());
            } else {
                std::cerr << "  Unknown command.\n";
            }
        }

        std::cout << "[Client " << clientOrdinal << "] Done.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
