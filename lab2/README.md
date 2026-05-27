# Лабораторная работа 2

Тема: создание потоков.

```powershell
cmake -S . -B build-lab2
cmake --build build-lab2 --config Release
build-lab2\bin\Release\Lab2.exe
```

Программа создает потоки `min_max` и `average`, ожидает их завершения и заменяет минимальные/максимальные элементы массива средним значением.
