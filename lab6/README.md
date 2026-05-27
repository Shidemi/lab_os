# Лабораторная работа 6

Java-версия задачи `marker` из лабораторной 3.

```bash
javac -encoding UTF-8 -d build/lab6/classes src/main/java/lab3/*.java
java -cp build/lab6/classes lab3.Main
```

Синхронизация реализована через `CountDownLatch` для общего старта и `Semaphore` для сигналов `stuck`/`continue`/`stop`.
