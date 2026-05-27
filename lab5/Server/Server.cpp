// Server.cpp
// Lab 5 - Named Pipes: File Access Management
//
// Server:
//   1. Creates the binary employee file from console input.
//   2. Pre-creates one named pipe per client.
//   3. Launches N Client processes.
//   4. Accepts connections and spawns a service thread per client.
//   5. Enforces per-record readers-writer locking.
//   6. After all clients disconnect, prints the modified file and saves it.
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <cstring>
#include <mutex>
#include <atomic>
#include <cstdlib>

#include "../include/protocol.h"
#include "../include/utils.h"
#include "../include/rw_lock.h"

// --- Constants ---------------------------------------------------------------

static const int   kMinRecords  = 1;
static const int   kMaxRecords  = 1000;
static const int   kMinClients  = 1;
static const int   kMaxClients  = 10;
static const char* kClientExe   = "Client.exe";

// --- Global in-memory database ----------------------------------------------
// gRecords is read/written by service threads; structural access (FindEmployee)
// is safe after initial load because the vector is never resized again.
// Per-record mutations are protected by per-ID RwLock instances.

static std::vector<Employee>                            gRecords;
static std::unordered_map<int, std::unique_ptr<RwLock>> gLocks;
static std::mutex                                       gLockMapMutex;
static std::atomic<int>                                 gActiveClients(0);

static RwLock& GetLock(int id) {
    std::lock_guard<std::mutex> guard(gLockMapMutex);
    auto it = gLocks.find(id);
    if (it == gLocks.end()) {
        gLocks[id] = std::make_unique<RwLock>();
        return *gLocks[id];
    }
    return *it->second;
}

static Employee* FindEmployee(int id) {
    for (Employee& emp : gRecords) {
        if (emp.num == id) return &emp;
    }
    return nullptr;
}

static bool EmployeeIdExistsBefore(int id, int lastExclusive) {
    for (int i = 0; i < lastExclusive; ++i) {
        if (gRecords[static_cast<size_t>(i)].num == id) {
            return true;
        }
    }
    return false;
}

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

static double PromptDouble(const std::string& prompt, double lo) {
    while (true) {
        std::cout << prompt;
        double v;
        if (std::cin >> v && v >= lo) return v;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cerr << "  Enter a number greater than or equal to " << lo << ".\n";
    }
}

static std::string PromptLine(const std::string& prompt) {
    std::cout << prompt;
    if (std::cin.peek() == '\n') std::cin.ignore();
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// --- File I/O ----------------------------------------------------------------

static void SaveEmployeesToFile(const std::string& filename) {
    std::ofstream f(filename, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot write file '" + filename + "'");
    for (const Employee& emp : gRecords) {
        f.write(reinterpret_cast<const char*>(&emp), sizeof(Employee));
    }
}

static void PrintEmployees(const std::string& header) {
    std::cout << "\n" << header << "\n";
    std::cout << std::left
              << std::setw(8)  << "ID"
              << std::setw(12) << "Name"
              << std::setw(10) << "Hours" << "\n";
    std::cout << std::string(30, '-') << "\n";
    for (const Employee& emp : gRecords) {
        std::cout << std::left
                  << std::setw(8)  << emp.num
                  << std::setw(12) << emp.name
                  << std::fixed << std::setprecision(2)
                  << std::setw(10) << emp.hours << "\n";
    }
    std::cout << "\n";
}

static void ReadEmployeesFromConsole(int count) {
    gRecords.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Employee& emp = gRecords[static_cast<size_t>(i)];
        std::cout << "--- Employee " << (i + 1) << " ---\n";
        while (true) {
            emp.num = PromptInt("  ID: ", 1, std::numeric_limits<int>::max());
            if (!EmployeeIdExistsBefore(emp.num, i)) {
                break;
            }
            std::cerr << "  Employee ID already exists. Enter another ID.\n";
        }

        std::string name;
        std::cout << "  Name (max 9 chars): ";
        std::cin >> name;
        if (name.size() >= sizeof(emp.name)) {
            name.resize(sizeof(emp.name) - 1);
            std::cout << "  Name truncated to '" << name << "'.\n";
        }
        std::strncpy(emp.name, name.c_str(), sizeof(emp.name) - 1);
        emp.name[sizeof(emp.name) - 1] = '\0';
        emp.hours = PromptDouble("  Hours: ", 0.0);
    }
}

// --- Pipe I/O helpers --------------------------------------------------------

static bool ReadExact(HANDLE pipe, void* buf, DWORD bytes) {
    DWORD total = 0;
    while (total < bytes) {
        DWORD read = 0;
        if (!ReadFile(pipe, static_cast<char*>(buf) + total, bytes - total, &read, nullptr)
            || read == 0) return false;
        total += read;
    }
    return true;
}

static bool WriteExact(HANDLE pipe, const void* buf, DWORD bytes) {
    DWORD written = 0;
    return WriteFile(pipe, buf, bytes, &written, nullptr) && written == bytes;
}

static bool RecvRequest(HANDLE pipe, Request& req) {
    return ReadExact(pipe, &req, sizeof(Request));
}

static bool SendResponse(HANDLE pipe, const Response& resp) {
    return WriteExact(pipe, &resp, sizeof(Response));
}

// --- Request dispatcher ------------------------------------------------------

// Tracks what lock (if any) a client currently holds, so we can release it
// on unexpected disconnect or QUIT.
struct ClientLockState {
    int  lockedId   = -1;
    bool isWrite    = false;
    bool hasLock    = false;
};

static void DispatchRequest(HANDLE pipe, const Request& req, ClientLockState& ls) {
    Response resp;
    resp.status = Status::OK;
    std::memset(resp.message, 0, sizeof(resp.message));

    switch (req.op) {

    case Operation::READ: {
        Employee* emp = FindEmployee(req.employeeId);
        if (nullptr == emp) {
            resp.status = Status::NOT_FOUND;
            std::strncpy(resp.message, "Employee not found", sizeof(resp.message) - 1);
            SendResponse(pipe, resp);
            break;
        }
        GetLock(req.employeeId).LockRead();
        ls = { req.employeeId, false, true };
        resp.record = *emp;
        SendResponse(pipe, resp);
        break;
    }

    case Operation::RELEASE_READ: {
        if (ls.hasLock && !ls.isWrite && ls.lockedId == req.employeeId) {
            GetLock(req.employeeId).UnlockRead();
            ls.hasLock = false;
        }
        SendResponse(pipe, resp);
        break;
    }

    case Operation::LOCK_WRITE: {
        Employee* emp = FindEmployee(req.employeeId);
        if (nullptr == emp) {
            resp.status = Status::NOT_FOUND;
            std::strncpy(resp.message, "Employee not found", sizeof(resp.message) - 1);
            SendResponse(pipe, resp);
            break;
        }
        // Blocks until no readers and no other writers hold this record
        GetLock(req.employeeId).LockWrite();
        ls = { req.employeeId, true, true };
        resp.record = *emp;
        SendResponse(pipe, resp);
        break;
    }

    case Operation::WRITE: {
        Employee* emp = FindEmployee(req.employeeId);
        if (nullptr == emp) {
            resp.status = Status::NOT_FOUND;
            std::strncpy(resp.message, "Employee not found", sizeof(resp.message) - 1);
        } else if (!ls.hasLock || !ls.isWrite || ls.lockedId != req.employeeId) {
            resp.status = Status::ERROR_GENERIC;
            std::strncpy(resp.message, "Write lock is not held", sizeof(resp.message) - 1);
        } else {
            Employee updated = req.record;
            updated.num = req.employeeId;
            *emp = updated;
            std::strncpy(resp.message, "Record updated", sizeof(resp.message) - 1);
        }
        SendResponse(pipe, resp);
        break;
    }

    case Operation::RELEASE_WRITE: {
        if (ls.hasLock && ls.isWrite && ls.lockedId == req.employeeId) {
            GetLock(req.employeeId).UnlockWrite();
            ls.hasLock = false;
        }
        SendResponse(pipe, resp);
        break;
    }

    case Operation::QUIT:
        // Release any held lock on clean exit
        if (ls.hasLock) {
            if (ls.isWrite) GetLock(ls.lockedId).UnlockWrite();
            else            GetLock(ls.lockedId).UnlockRead();
            ls.hasLock = false;
        }
        break;

    default:
        resp.status = Status::ERROR_GENERIC;
        std::strncpy(resp.message, "Unknown operation", sizeof(resp.message) - 1);
        SendResponse(pipe, resp);
        break;
    }
}

// --- Per-client service thread -----------------------------------------------

struct ClientThreadParams {
    HANDLE pipe;
    int    clientOrdinal;
};

DWORD WINAPI ClientServiceThread(LPVOID param) {
    ClientThreadParams* p = reinterpret_cast<ClientThreadParams*>(param);
    HANDLE pipe    = p->pipe;
    int    ordinal = p->clientOrdinal;
    delete p;

    std::cout << "[Server] Client " << ordinal << " connected.\n";

    ClientLockState lockState;
    Request req;

    while (RecvRequest(pipe, req)) {
        DispatchRequest(pipe, req, lockState);
        if (req.op == Operation::QUIT) break;
    }

    // Safety release in case of unexpected disconnect
    if (lockState.hasLock) {
        if (lockState.isWrite) GetLock(lockState.lockedId).UnlockWrite();
        else                   GetLock(lockState.lockedId).UnlockRead();
    }

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);

    std::cout << "[Server] Client " << ordinal << " disconnected.\n";
    --gActiveClients;
    return EXIT_SUCCESS;
}

// --- Launch clients ----------------------------------------------------------

static void LaunchClients(int clientCount) {
    for (int i = 1; i <= clientCount; ++i) {
        std::string pipeName = std::string(kPipePrefix) + std::to_string(i);
        std::ostringstream oss;
        oss << kClientExe << " " << i << " \"" << pipeName << "\"";
        std::string cmd = oss.str();

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        std::memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
        std::memset(&pi, 0, sizeof(pi));

        if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr,
                            FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
            ThrowLastError("CreateProcess failed for Client " + std::to_string(i));
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        std::cout << "[Server] Client " << i << " launched.\n";
    }
}

// --- Entry point -------------------------------------------------------------

int main() {
    try {
        std::cout << "=== Lab 5: Named Pipes - File Access Server ===\n\n";

        // 1.1: create employee file
        std::string filename = PromptLine("Enter binary file name: ");
        if (filename.empty()) throw std::invalid_argument("File name cannot be empty");

        int recordCount = PromptInt("Enter number of employees: ", kMinRecords, kMaxRecords);
        ReadEmployeesFromConsole(recordCount);
        SaveEmployeesToFile(filename);

        // 1.2: display the file
        PrintEmployees("=== Initial file contents ===");

        // 1.3: ask how many clients
        int clientCount = PromptInt(
            "Enter number of Client processes: ", kMinClients, kMaxClients
        );

        // Pre-create all named pipe instances BEFORE launching clients
        // so the pipe exists when the client calls CreateFile
        std::vector<HANDLE> pipes(static_cast<size_t>(clientCount), INVALID_HANDLE_VALUE);
        for (int i = 0; i < clientCount; ++i) {
            std::string pipeName = std::string(kPipePrefix) + std::to_string(i + 1);
            pipes[static_cast<size_t>(i)] = CreateNamedPipeA(
                pipeName.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,              // max instances (1 per pipe name)
                kPipeBufferSz,
                kPipeBufferSz,
                kPipeTimeout,
                nullptr
            );
            if (INVALID_HANDLE_VALUE == pipes[static_cast<size_t>(i)]) {
                ThrowLastError("CreateNamedPipe failed for client " + std::to_string(i + 1));
            }
        }

        // Launch client processes
        LaunchClients(clientCount);

        // Accept connections one by one; spawn a service thread for each
        for (int i = 0; i < clientCount; ++i) {
            HANDLE pipe = pipes[static_cast<size_t>(i)];
            std::cout << "[Server] Waiting for Client " << (i + 1) << " to connect...\n";

            if (!ConnectNamedPipe(pipe, nullptr)) {
                DWORD err = GetLastError();
                if (ERROR_PIPE_CONNECTED != err) {
                    CloseHandle(pipe);
                    ThrowLastError("ConnectNamedPipe failed for client " + std::to_string(i + 1));
                }
            }

            ++gActiveClients;

            ClientThreadParams* params = new ClientThreadParams{ pipe, i + 1 };
            HANDLE thread = CreateThread(nullptr, 0, ClientServiceThread, params, 0, nullptr);
            if (NULL == thread) {
                delete params;
                CloseHandle(pipe);
                ThrowLastError("CreateThread failed for client " + std::to_string(i + 1));
            }
            CloseHandle(thread); // detach; the thread manages its own pipe handle lifetime
        }

        // 1.4: wait for all clients to finish
        // Press Enter in the server window to check remaining client count.
        std::cout << "\n[Server] Serving clients. Press Enter to check active count.\n";
        while (gActiveClients.load() > 0) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            int remaining = gActiveClients.load();
            if (remaining > 0) {
                std::cout << "[Server] " << remaining << " client(s) still active.\n";
            }
        }

        // 1.5: display and save the modified file
        PrintEmployees("=== Modified file contents ===");
        SaveEmployeesToFile(filename);

        // 1.6: done
        std::cout << "[Server] All clients done. Press Enter to exit.\n";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
