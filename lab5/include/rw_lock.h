#pragma once

// Per-record readers-writer lock.
//
// Rules:
//   - Several readers may hold one record at the same time.
//   - A writer gets exclusive access.
//   - Waiting writers have priority over new readers to avoid writer starvation.

#include <windows.h>
#include <stdexcept>

class RwLock {
public:
    RwLock()
        : readerCount_(0),
          waitingWriters_(0),
          writerActive_(false) {
        InitializeCriticalSection(&cs_);
        InitializeConditionVariable(&canRead_);
        InitializeConditionVariable(&canWrite_);
    }

    ~RwLock() {
        DeleteCriticalSection(&cs_);
    }

    void LockRead() {
        EnterCriticalSection(&cs_);
        while (writerActive_ || waitingWriters_ > 0) {
            if (!SleepConditionVariableCS(&canRead_, &cs_, INFINITE)) {
                LeaveCriticalSection(&cs_);
                throw std::runtime_error("RwLock: read wait failed");
            }
        }
        ++readerCount_;
        LeaveCriticalSection(&cs_);
    }

    void UnlockRead() {
        EnterCriticalSection(&cs_);
        if (readerCount_ <= 0) {
            LeaveCriticalSection(&cs_);
            throw std::runtime_error("RwLock: read unlock without lock");
        }
        --readerCount_;
        if (0 == readerCount_) {
            WakeConditionVariable(&canWrite_);
        }
        LeaveCriticalSection(&cs_);
    }

    void LockWrite() {
        EnterCriticalSection(&cs_);
        ++waitingWriters_;
        while (writerActive_ || readerCount_ > 0) {
            if (!SleepConditionVariableCS(&canWrite_, &cs_, INFINITE)) {
                --waitingWriters_;
                LeaveCriticalSection(&cs_);
                throw std::runtime_error("RwLock: write wait failed");
            }
        }
        --waitingWriters_;
        writerActive_ = true;
        LeaveCriticalSection(&cs_);
    }

    void UnlockWrite() {
        EnterCriticalSection(&cs_);
        if (!writerActive_) {
            LeaveCriticalSection(&cs_);
            throw std::runtime_error("RwLock: write unlock without lock");
        }
        writerActive_ = false;
        if (waitingWriters_ > 0) {
            WakeConditionVariable(&canWrite_);
        } else {
            WakeAllConditionVariable(&canRead_);
        }
        LeaveCriticalSection(&cs_);
    }

    RwLock(const RwLock&) = delete;
    RwLock& operator=(const RwLock&) = delete;

private:
    CRITICAL_SECTION  cs_;
    CONDITION_VARIABLE canRead_;
    CONDITION_VARIABLE canWrite_;
    int               readerCount_;
    int               waitingWriters_;
    bool              writerActive_;
};

class ReadGuard {
public:
    explicit ReadGuard(RwLock& lock) : lock_(lock) { lock_.LockRead(); }
    ~ReadGuard() { lock_.UnlockRead(); }
    ReadGuard(const ReadGuard&) = delete;
    ReadGuard& operator=(const ReadGuard&) = delete;

private:
    RwLock& lock_;
};

class WriteGuard {
public:
    explicit WriteGuard(RwLock& lock) : lock_(lock) { lock_.LockWrite(); }
    ~WriteGuard() { lock_.UnlockWrite(); }
    WriteGuard(const WriteGuard&) = delete;
    WriteGuard& operator=(const WriteGuard&) = delete;

private:
    RwLock& lock_;
};
