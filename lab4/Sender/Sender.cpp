// Sender.cpp
// Lab 4 - Inter-Process Communication via shared binary file (circular FIFO queue)
//
// Sender:
//   1. Opens the queue file (name from command line).
//   2. Opens named sync objects created by Receiver.
//   3. Signals "ready" to Receiver.
//   4. Sends messages on demand (console command 's' = send, 'q' = quit).
//      Blocks if the queue is full.

#include <windows.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <limits>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include "../include/shared.h"
#include "../include/win_error.h"
#include "../include/scoped_handle.h"
#include "../include/queue_io.h"

// --- Input helpers -----------------------------------------------------------

static std::string PromptLine(const std::string& prompt) {
    std::cout << prompt;
    if (std::cin.peek() == '\n') std::cin.ignore();
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// Reads a message from the console, truncating to kMaxMessageLen chars
static std::string ReadMessage() {
    while (true) {
        std::string msg = PromptLine("  Enter message (max " +
                                     std::to_string(kMaxMessageLen) + " chars): ");
        if (msg.empty()) {
            std::cerr << "  Message cannot be empty.\n";
            continue;
        }
        if (static_cast<int>(msg.size()) > kMaxMessageLen) {
            msg.resize(static_cast<size_t>(kMaxMessageLen));
            std::cout << "  Message truncated to: \"" << msg << "\"\n";
        }
        return msg;
    }
}

// --- Named sync objects ------------------------------------------------------

struct SyncObjects {
    ScopedHandle mutex;
    ScopedHandle semItems;
    ScopedHandle semSpace;
    ScopedHandle readyEvent;
};

static void OpenSyncObjects(SyncObjects& sync, int senderOrdinal) {
    sync.mutex = ScopedHandle(OpenMutexA(MUTEX_ALL_ACCESS, FALSE, kMutexName));
    if (NULL == sync.mutex.get()) ThrowLastError("OpenMutex failed");

    sync.semItems = ScopedHandle(
        OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, kSemItemsName)
    );
    if (NULL == sync.semItems.get()) ThrowLastError("OpenSemaphore(items) failed");

    sync.semSpace = ScopedHandle(
        OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, kSemSpaceName)
    );
    if (NULL == sync.semSpace.get()) ThrowLastError("OpenSemaphore(space) failed");

    std::string evName = std::string(kReadyEventPrefix) + std::to_string(senderOrdinal);
    sync.readyEvent = ScopedHandle(
        OpenEventA(EVENT_ALL_ACCESS, FALSE, evName.c_str())
    );
    if (NULL == sync.readyEvent.get()) ThrowLastError("OpenEvent failed for " + evName);
}

// --- Entry point -------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: Sender.exe <queue_file> <sender_ordinal>\n";
        return EXIT_FAILURE;
    }

    try {
        const std::string queueFilename = argv[1];
        int senderOrdinal = std::stoi(argv[2]);

        std::cout << "=== Sender " << senderOrdinal
                  << " (queue: " << queueFilename << ") ===\n\n";

        // 1. Verify the queue file is accessible
        std::FILE* testOpen = std::fopen(queueFilename.c_str(), "r+b");
        if (nullptr == testOpen) {
            throw std::runtime_error("Cannot open queue file '" + queueFilename + "'");
        }
        std::fclose(testOpen);

        // 2. Open named sync objects (created by Receiver)
        SyncObjects sync;
        OpenSyncObjects(sync, senderOrdinal);

        // 3. Signal ready to Receiver
        SetEvent(sync.readyEvent.get());
        std::cout << "[Sender " << senderOrdinal << "] Ready signal sent.\n";

        // 4. Send messages on demand
        std::cout << "Commands:  s = send message   q = quit\n";

        while (true) {
            std::cout << "[S" << senderOrdinal << "]> ";
            std::string cmd;
            std::cin >> cmd;

            if (cmd == "q" || cmd == "Q") {
                std::cout << "[Sender " << senderOrdinal << "] Quitting.\n";
                break;
            }

            if (cmd != "s" && cmd != "S") {
                std::cerr << "  Unknown command. Use 's' or 'q'.\n";
                continue;
            }

            std::string text = ReadMessage();

            // Block until there is free space in the queue
            std::cout << "[Sender " << senderOrdinal << "] Waiting for free slot...\n";
            WaitForSingleObject(sync.semSpace.get(), INFINITE);

            // Lock file, enqueue, unlock
            WaitForSingleObject(sync.mutex.get(), INFINITE);

            std::FILE* f = std::fopen(queueFilename.c_str(), "r+b");
            if (nullptr == f) {
                ReleaseMutex(sync.mutex.get());
                ReleaseSemaphore(sync.semSpace.get(), 1, nullptr); // give back the slot
                throw std::runtime_error("Cannot open queue file for writing");
            }

            MessageRecord rec;
            std::strncpy(rec.text, text.c_str(), kMaxMessageLen);
            rec.text[kMaxMessageLen] = '\0';
            rec.senderId = senderOrdinal;

            try {
                EnqueueMessage(f, rec);
            } catch (...) {
                std::fclose(f);
                ReleaseMutex(sync.mutex.get());
                ReleaseSemaphore(sync.semSpace.get(), 1, nullptr);
                throw;
            }

            std::fclose(f);
            ReleaseMutex(sync.mutex.get());

            // Notify Receiver that a new message is available
            ReleaseSemaphore(sync.semItems.get(), 1, nullptr);

            std::cout << "[Sender " << senderOrdinal << "] Message sent: \""
                      << text << "\"\n";
        }

        // ScopedHandles released in reverse acquisition order by destructors

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
