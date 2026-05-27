#pragma once

#include <windows.h>
#include <cstring>

// ---- Employee record --------------------------------------------------------

struct Employee {
    int    num;       // Employee ID (used as record key)
    char   name[10];  // Employee name (max 9 chars + null)
    double hours;     // Hours worked

    Employee() {
        num = 0;
        std::memset(name, 0, sizeof(name));
        hours = 0.0;
    }
};

// ---- Request / Response protocol over the named pipe -----------------------
//
// Client -> Server:  Request
// Server -> Client:  Response
//
// Flow for READ:
//   Client sends  Request  { op=READ,  employeeId=X }
//   Server sends  Response { status=OK, record=<data> }
//   Client sends  Request  { op=RELEASE_READ, employeeId=X }
//
// Flow for MODIFY:
//   Client sends  Request  { op=LOCK_WRITE, employeeId=X }
//   Server sends  Response { status=OK, record=<data> }     (record locked for others)
//   Client sends  Request  { op=WRITE,  employeeId=X, record=<modified> }
//   Server sends  Response { status=OK }
//   Client sends  Request  { op=RELEASE_WRITE, employeeId=X }
//   Server sends  Response { status=OK }
//
// Flow for EXIT:
//   Client sends  Request  { op=QUIT }
//   (no response expected)

enum class Operation : int {
    READ          = 1,
    RELEASE_READ  = 2,
    LOCK_WRITE    = 3,
    WRITE         = 4,
    RELEASE_WRITE = 5,
    QUIT          = 6,
};

enum class Status : int {
    OK            = 0,
    NOT_FOUND     = 1,
    LOCKED        = 2,
    ERROR_GENERIC = 3,
};

struct Request {
    Operation op = Operation::QUIT;
    int       employeeId = 0; // Key to look up; unused for QUIT
    Employee  record;         // Only populated for WRITE
};

struct Response {
    Status   status = Status::ERROR_GENERIC;
    Employee record;          // Only populated for READ / LOCK_WRITE responses
    char     message[64] = {}; // Human-readable status detail
};

// ---- Named pipe constants ---------------------------------------------------

// Each client gets its own pipe instance: \\.\pipe\Lab5_Pipe_<clientOrdinal>
static const char* kPipePrefix    = "\\\\.\\pipe\\Lab5_Pipe_";
static const DWORD kPipeBufferSz  = 4096;
static const DWORD kPipeTimeout   = 5000; // ms

// Server signals this event when all clients have connected
static const char* kAllReadyEvent = "Lab5_AllClientsReady";
