#pragma once

#include <vector>

// Data shared between the main thread and worker threads.
// Each thread receives a pointer to this struct.
struct SharedData {
    std::vector<int> array;   // The integer array (read-only for worker threads)
    int    minValue;          // Result from min_max thread
    int    maxValue;          // Result from min_max thread
    double average;           // Result from average thread
};
