# Лабораторная работа 1

Тема: создание процессов.

```powershell
cmake -S . -B build-lab1
cmake --build build-lab1 --config Release
build-lab1\bin\Release\Main.exe
```

`Main` запускает `Creator` и `Reporter`, ожидает их через `WaitForSingleObject`, выводит бинарный файл и сформированный отчет.
