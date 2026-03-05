#include <iostream>
#include <fstream>
#include <cstring>
#include "employee.h"

using namespace std;

int main(int argc, char* argv[])
{
    // check if program received parameters
    if (argc < 3)
    {
        cout << "Not enough arguments\n";
        cout << "Usage: Creator <file> <count>\n";
        return 1;
    }

    const char* fileName = argv[1];
    int count = atoi(argv[2]);

    // open file in binary mode
    ofstream fout(fileName, ios::binary);

    if (!fout.is_open())
    {
        cout << "Cannot create file\n";
        return 2;
    }

    employee emp{};
    
    // write employees one by one
    for (int i = 0; i < count; i++)
    {
        cout << "\nEmployee " << i + 1 << endl;

        cout << "Enter ID: ";
        cin >> emp.num;

        cout << "Enter name: ";
        cin >> emp.name;

        cout << "Enter hours: ";
        cin >> emp.hours;

        // write structure directly into binary file
        fout.write(reinterpret_cast<char*>(&emp), sizeof(emp));
    }

    fout.close();

    cout << "\nFile successfully created\n";

    return 0;
}