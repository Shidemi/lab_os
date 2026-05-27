#pragma once

#include <cstring>

// ---- Message record stored in the binary file --------------------------------

static const int kMaxMessageLen  = 20;   // Max chars in a message (excl. null)
static const int kMaxQueueSlots  = 16;   // Maximum records in the binary file

// One record in the binary queue file.
// Fields are plain POD so they can be written/read with fwrite/fread.
struct MessageRecord {
    char text[kMaxMessageLen + 1]; // Null-terminated message text
    int  senderId;                 // Ordinal of the Sender that wrote this record

    MessageRecord() {
        std::memset(text, 0, sizeof(text));
        senderId = 0;
    }
};

// ---- Queue header stored at the beginning of the binary file ----------------

// The file layout:
//   [ QueueHeader ][ MessageRecord * capacity ]
//
// head  = index of next record to READ  (Receiver advances this)
// tail  = index of next slot  to WRITE (Sender  advances this)
// count = number of messages currently in the queue
struct QueueHeader {
    int head;
    int tail;
    int count;
    int capacity; // total number of MessageRecord slots

    QueueHeader() : head(0), tail(0), count(0), capacity(0) {}
};

// ---- Named synchronization object names (global, so all processes agree) ----

// Mutex protecting all file I/O (one writer or reader at a time)
static const char* kMutexName          = "Lab4_QueueMutex";

// Semaphore: counts available messages (Receiver waits on this)
static const char* kSemItemsName       = "Lab4_SemItems";

// Semaphore: counts free slots (Sender waits on this)
static const char* kSemSpaceName       = "Lab4_SemSpace";

// Event: each Sender signals "ready"; Receiver waits for all of them
// Individual per-sender events are named "Lab4_ReadyEvent_N" (N = sender ordinal)
static const char* kReadyEventPrefix   = "Lab4_ReadyEvent_";
