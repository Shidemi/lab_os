#pragma once // Include guard (platform-specific but widely supported)

// Employee record structure shared between Creator, Reporter, and Main.
struct Employee {
    int    num = 0;       // Employee identification number
    char   name[10] = {}; // Employee name (max 9 chars + null terminator)
    double hours = 0.0;   // Number of hours worked
};
