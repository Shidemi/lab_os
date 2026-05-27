# Лабораторная работа 5

Тема: обмен данными по именованным каналам.

```powershell
cmake -S . -B build-lab5
cmake --build build-lab5 --config Release
build-lab5\bin\Release\Server.exe
```

`Server` обслуживает процессы `Client` через named pipes и управляет доступом к записям сотрудников с помощью readers-writer lock.
