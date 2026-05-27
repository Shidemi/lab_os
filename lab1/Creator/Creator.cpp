// Creator.cpp
// Utility that creates a binary file of Employee records from console input.
// Usage: Creator.exe <filename> <record_count>

#include <windows.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <cstring>
#include <cstdlib>

#include "../include/employee.h"
#include "../include/win_error.h"

static const int kMinRecordCount = 1;
static const int kMaxRecordCount = 10000;

// Validates that the record count argument is within acceptable bounds
static int ParseRecordCount(const char* arg) {
    try {
        int count = std::stoi(arg);
        if (count < kMinRecordCount || count > kMaxRecordCount) {
            throw std::out_of_range("Record count out of range");
        }
        return count;
    } catch (const std::exception&) {
        throw std::invalid_argument(
            std::string("Invalid record count '") + arg +
            "'. Must be an integer between " +
            std::to_string(kMinRecordCount) + " and " +
            std::to_string(kMaxRecordCount) + "."
        );
    }
}

// Reads a single Employee record from the console
static Employee ReadEmployeeFromConsole(int index) {
    Employee emp;
    std::memset(&emp, 0, sizeof(Employee));

    std::cout << "--- Employee #" << (index + 1) << " ---\n";

    std::cout << "  ID number : ";
    if (!(std::cin >> emp.num)) {
        throw std::runtime_error("Failed to read employee ID");
    }

    std::cout << "  Name (max 9 chars): ";
    std::string name;
    if (!(std::cin >> name)) {
        throw std::runtime_error("Failed to read employee name");
    }
    if (name.size() >= sizeof(emp.name)) {
        name.resize(sizeof(emp.name) - 1);
        std::cout << "  Name truncated to '" << name << "'.\n";
    }
    std::strncpy(emp.name, name.c_str(), sizeof(emp.name) - 1);
    emp.name[sizeof(emp.name) - 1] = '\0';

    std::cout << "  Hours worked: ";
    if (!(std::cin >> emp.hours)) {
        throw std::runtime_error("Failed to read hours worked");
    }
    if (emp.hours < 0.0) {
        throw std::invalid_argument("Hours worked cannot be negative");
    }

    return emp;
}

// Writes all employee records to the binary file
static void WriteEmployeesToFile(const std::string& filename, int recordCount) {
    // Check if file already exists
    std::ifstream existCheck(filename, std::ios::binary);
    if (existCheck.good()) {
        existCheck.close();
        std::cout << "Warning: file '" << filename << "' already exists and will be overwritten.\n";
    }

    std::ofstream outFile(filename, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        throw std::runtime_error("Cannot create file '" + filename + "'");
    }

    std::cout << "Enter employee data (" << recordCount << " records):\n";

    Employee emp;
    for (int i = 0; i < recordCount; ++i) {
        emp = ReadEmployeeFromConsole(i);
        outFile.write(reinterpret_cast<const char*>(&emp), sizeof(Employee));
        if (!outFile) {
            throw std::runtime_error("Write error: disk may be full");
        }
    }

    outFile.close();
    std::cout << "Binary file '" << filename << "' created successfully with "
              << recordCount << " record(s).\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: Creator.exe <filename> <record_count>\n";
        return EXIT_FAILURE;
    }

    try {
        const std::string filename = argv[1];
        int recordCount = ParseRecordCount(argv[2]);
        WriteEmployeesToFile(filename, recordCount);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Input error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
