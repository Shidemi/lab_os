#include <iostream>
#include <fstream>
#include "employee.h"

using namespace std;

int main(int argc, char* argv[])
{
    // program should receive 3 parameters
    if (argc < 4)
    {
        cout << "Usage: Reporter <binfile> <reportfile> <hourpay>\n";
        return 1;
    }

    const char* binFile = argv[1];
    const char* reportFile = argv[2];
    double payPerHour = atof(argv[3]);

    ifstream fin(binFile, ios::binary);

    if (!fin)
    {
        cout << "Cannot open binary file\n";
        return 2;
    }

    ofstream fout(reportFile);

    if (!fout)
    {
        cout << "Cannot create report file\n";
        return 3;
    }

    // report header
    fout << "Report for file \"" << binFile << "\"" << endl;
    fout << "ID\tName\tHours\tSalary\n";

    employee emp{};

    // read all records until file ends
    while (fin.read(reinterpret_cast<char*>(&emp), sizeof(emp)))
    {
        double salary = emp.hours * payPerHour;

        fout << emp.num << "\t"
             << emp.name << "\t"
             << emp.hours << "\t"
             << salary << endl;
    }

    fin.close();
    fout.close();

    cout << "Report created\n";

    return 0;
}