#pragma once

// queue_io.h
// Low-level helpers for reading and writing the circular FIFO queue stored
// in a binary file.  All callers must hold the kMutexName mutex before
// calling any function here.

#include <cstdio>
#include <stdexcept>
#include <string>
#include "shared.h"

// Reads the queue header from an open file (seeks to offset 0)
inline QueueHeader ReadHeader(std::FILE* f) {
    QueueHeader hdr;
    std::rewind(f);
    if (std::fread(&hdr, sizeof(QueueHeader), 1, f) != 1) {
        throw std::runtime_error("Failed to read queue header");
    }
    return hdr;
}

// Writes the queue header back to an open file (seeks to offset 0)
inline void WriteHeader(std::FILE* f, const QueueHeader& hdr) {
    std::rewind(f);
    if (std::fwrite(&hdr, sizeof(QueueHeader), 1, f) != 1) {
        throw std::runtime_error("Failed to write queue header");
    }
    std::fflush(f);
}

// Returns the file offset (bytes) of record slot [index]
inline long RecordOffset(int index) {
    return static_cast<long>(sizeof(QueueHeader)) +
           static_cast<long>(index) * static_cast<long>(sizeof(MessageRecord));
}

// Enqueues a message at the tail slot (caller must have checked there is space)
inline void EnqueueMessage(std::FILE* f, const MessageRecord& rec) {
    QueueHeader hdr = ReadHeader(f);
    if (hdr.count >= hdr.capacity) {
        throw std::runtime_error("Queue is full: cannot enqueue");
    }
    std::fseek(f, RecordOffset(hdr.tail), SEEK_SET);
    if (std::fwrite(&rec, sizeof(MessageRecord), 1, f) != 1) {
        throw std::runtime_error("Failed to write message record");
    }
    hdr.tail = (hdr.tail + 1) % hdr.capacity;
    ++hdr.count;
    WriteHeader(f, hdr);
}

// Dequeues the oldest message from the head slot (caller must have checked there is a message)
inline MessageRecord DequeueMessage(std::FILE* f) {
    QueueHeader hdr = ReadHeader(f);
    if (hdr.count <= 0) {
        throw std::runtime_error("Queue is empty: cannot dequeue");
    }
    MessageRecord rec;
    std::fseek(f, RecordOffset(hdr.head), SEEK_SET);
    if (std::fread(&rec, sizeof(MessageRecord), 1, f) != 1) {
        throw std::runtime_error("Failed to read message record");
    }
    hdr.head = (hdr.head + 1) % hdr.capacity;
    --hdr.count;
    WriteHeader(f, hdr);
    return rec;
}
