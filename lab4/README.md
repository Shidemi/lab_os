# Лабораторная работа 4

Тема: синхронизация процессов через общий файл.

```powershell
cmake -S . -B build-lab4
cmake --build build-lab4 --config Release
build-lab4\bin\Release\Receiver.exe
```

`Receiver` создает бинарную FIFO-очередь сообщений и запускает процессы `Sender`. Синхронизация выполнена через именованные WinAPI mutex, semaphore и event.
