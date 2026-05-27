#pragma once

#include <windows.h>
#include <vector>
#include <cstddef>

// Per-thread synchronization events for one marker thread.
// Owned and initialized by main; passed to the thread via MarkerParams.
struct MarkerEvents {
    HANDLE startEvent;    // Manual-reset: main signals "go" to all markers at once
    HANDLE stuckEvent;    // Auto-reset:  marker signals main "I am stuck"
    HANDLE continueEvent; // Auto-reset:  main signals marker "keep going"
    HANDLE stopEvent;     // Manual-reset: main signals marker "terminate"
};

// Parameters passed to each marker thread on creation.
struct MarkerParams {
    int            markerIndex; // 1-based ordinal assigned to this thread
    int*           array;       // Pointer to the shared array
    int            arraySize;   // Size of the shared array
    CRITICAL_SECTION* cs;       // Protects array writes
    MarkerEvents   events;      // Synchronization events for this thread
};

// Aggregate program state owned by main.
struct ProgramState {
    std::vector<int>    array;           // The shared integer array
    CRITICAL_SECTION    cs;              // Protects array element writes
    int                 markerCount;     // Total number of marker threads
    HANDLE              startEvent;      // Broadcast: all markers start together
    std::vector<MarkerEvents> markerEvents; // Per-thread event sets
    std::vector<HANDLE>       threadHandles; // Raw handles (for WaitForMultipleObjects)
};
