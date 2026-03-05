#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include "employee.h"

using namespace std;

void showBinary(const char* fileName)
{
    ifstream fin(fileName, ios::binary);

    if (!fin)
    {
        cout << "Cannot open binary file\n";
        return;
    }

    employee emp{};

    cout << "\nBinary file content:\n";

    while (fin.read(reinterpret_cast<char*>(&emp), sizeof(emp)))
    {
        cout << emp.num << " "
             << emp.name << " "
             << emp.hours << endl;
    }

    fin.close();
}

// print report file to console
void showReport(const char* fileName)
{
    ifstream fin(fileName);

    if (!fin)
    {
        cout << "Cannot open report file\n";
        return;
    }

    string line;

    cout << "\nReport:\n";

    // output whole file line by line
    while (getline(fin, line))
    {
        cout << line << endl;
    }

    fin.close();
}

int main()
{
    char binFile[50];
    int count;

    cout << "Enter binary file name: ";
    cin >> binFile;

    cout << "Enter number of records: ";
    cin >> count;

    STARTUPINFO si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    char cmd[120];

    // build command line for Creator
    sprintf_s(cmd, "Creator.exe %s %d", binFile, count);

    // start Creator process
    if (!CreateProcess(
        NULL,
        cmd,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &pi))
    {
        cout << "Failed to start Creator\n";
        return 1;
    }

    // wait until Creator finishes (so program does not continue earlier)
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // show what was written to binary file
    showBinary(binFile);

    char reportFile[50];
    double pay;

    cout << "\nEnter report file name: ";
    cin >> reportFile;

    cout << "Enter pay per hour: ";
    cin >> pay;

    // build command for Reporter
    sprintf_s(cmd, "Reporter.exe %s %s %lf", binFile, reportFile, pay);

    if (!CreateProcess(
        NULL,
        cmd,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &pi))
    {
        cout << "Failed to start Reporter\n";
        return 2;
    }

    // wait again so program does not exit before report is created
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    showReport(reportFile);

    cout << "\nProgram finished\n";

    return 0;
}