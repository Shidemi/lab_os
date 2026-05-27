// Reporter.cpp
// Utility that reads a binary Employee file and produces a sorted text report.
// Usage: Reporter.exe <binary_file> <report_file> <hourly_rate>

#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <cstdlib>

#include "../include/employee.h"
#include "../include/win_error.h"

static const int kNameColumnWidth   = 12;
static const int kNumColumnWidth    = 8;
static const int kHoursColumnWidth  = 10;
static const int kSalaryColumnWidth = 12;

// Parses the hourly rate from a command-line argument
static double ParseHourlyRate(const char* arg) {
    try {
        double rate = std::stod(arg);
        if (rate < 0.0) {
            throw std::invalid_argument("Hourly rate cannot be negative");
        }
        return rate;
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument(
            std::string("Invalid hourly rate '") + arg + "'. Must be a non-negative number."
        );
    }
}

// Reads all Employee records from the binary file
static std::vector<Employee> ReadEmployeesFromFile(const std::string& filename) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile.is_open()) {
        throw std::runtime_error("Cannot open binary file '" + filename + "'");
    }

    std::vector<Employee> employees;
    Employee emp;
    while (inFile.read(reinterpret_cast<char*>(&emp), sizeof(Employee))) {
        employees.push_back(emp);
    }

    if (!inFile.eof() && inFile.fail()) {
        throw std::runtime_error("Read error in file '" + filename + "'");
    }

    return employees;
}

// Comparator: sort employees by name alphabetically
static bool CompareByName(const Employee& a, const Employee& b) {
    return std::string(a.name) < std::string(b.name);
}

// Writes the formatted report header
static void WriteReportHeader(std::ofstream& out, const std::string& sourceFilename) {
    out << "Report for file: \"" << sourceFilename << "\"\n";
    out << std::string(kNumColumnWidth + kNameColumnWidth + kHoursColumnWidth + kSalaryColumnWidth + 6, '-') << "\n";
    out << std::left
        << std::setw(kNumColumnWidth)    << "ID"
        << std::setw(kNameColumnWidth)   << "Name"
        << std::setw(kHoursColumnWidth)  << "Hours"
        << std::setw(kSalaryColumnWidth) << "Salary"
        << "\n";
    out << std::string(kNumColumnWidth + kNameColumnWidth + kHoursColumnWidth + kSalaryColumnWidth + 6, '-') << "\n";
}

// Writes a single employee row to the report
static void WriteEmployeeRow(std::ofstream& out, const Employee& emp, double hourlyRate) {
    double salary = emp.hours * hourlyRate;
    out << std::left
        << std::setw(kNumColumnWidth)    << emp.num
        << std::setw(kNameColumnWidth)   << emp.name
        << std::setw(kHoursColumnWidth)  << std::fixed << std::setprecision(2) << emp.hours
        << std::setw(kSalaryColumnWidth) << std::fixed << std::setprecision(2) << salary
        << "\n";
}

// Generates and writes the full sorted report to the output file
static void GenerateReport(
    const std::string& sourceFilename,
    const std::string& reportFilename,
    double hourlyRate
) {
    std::vector<Employee> employees = ReadEmployeesFromFile(sourceFilename);

    if (employees.empty()) {
        throw std::runtime_error("Binary file '" + sourceFilename + "' contains no records");
    }

    std::sort(employees.begin(), employees.end(), CompareByName);

    // Check if report file already exists
    std::ifstream existCheck(reportFilename);
    if (existCheck.good()) {
        existCheck.close();
        std::cout << "Warning: report file '" << reportFilename << "' already exists and will be overwritten.\n";
    }

    std::ofstream outFile(reportFilename);
    if (!outFile.is_open()) {
        throw std::runtime_error("Cannot create report file '" + reportFilename + "'");
    }

    WriteReportHeader(outFile, sourceFilename);

    for (const Employee& emp : employees) {
        WriteEmployeeRow(outFile, emp, hourlyRate);
        if (!outFile) {
            throw std::runtime_error("Write error: disk may be full");
        }
    }

    outFile.close();
    std::cout << "Report '" << reportFilename << "' created successfully.\n";
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: Reporter.exe <binary_file> <report_file> <hourly_rate>\n";
        return EXIT_FAILURE;
    }

    try {
        const std::string sourceFilename = argv[1];
        const std::string reportFilename = argv[2];
        double hourlyRate = ParseHourlyRate(argv[3]);
        GenerateReport(sourceFilename, reportFilename, hourlyRate);
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
