package lab3;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Scanner;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicIntegerArray;

/**
 * Java version of the marker-thread task from lab 3.
 *
 * Synchronization mapping:
 *   CRITICAL_SECTION                -> AtomicIntegerArray.compareAndSet
 *   manual-reset start event         -> CountDownLatch(1)
 *   per-marker "stuck" event         -> Semaphore(0)
 *   per-marker continue/stop command -> Semaphore(0) plus volatile flags
 */
public class Main {

    private static final int MIN_ARRAY_SIZE = 2;
    private static final int MAX_ARRAY_SIZE = 100_000;
    private static final int MIN_MARKER_COUNT = 1;
    private static final int MAX_MARKER_COUNT = 64;

    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);

        System.out.println("=== Lab 6: Marker Threads (Java) ===\n");

        int arraySize = promptInt(scanner, "Enter array size: ", MIN_ARRAY_SIZE, MAX_ARRAY_SIZE);
        AtomicIntegerArray array = new AtomicIntegerArray(arraySize);

        int markerCount = promptInt(
                scanner,
                "Enter number of marker threads: ",
                MIN_MARKER_COUNT,
                MAX_MARKER_COUNT
        );

        CountDownLatch startLatch = new CountDownLatch(1);
        List<Semaphore> stuckSignals = new ArrayList<>(markerCount);
        List<MarkerThread> markers = new ArrayList<>(markerCount);

        for (int i = 0; i < markerCount; i++) {
            Semaphore stuckSignal = new Semaphore(0);
            Semaphore commandSignal = new Semaphore(0);
            stuckSignals.add(stuckSignal);
            markers.add(new MarkerThread(i + 1, array, startLatch, stuckSignal, commandSignal));
        }

        for (MarkerThread marker : markers) {
            marker.start();
        }

        System.out.println("\n[main] Signalling all markers to start...");
        startLatch.countDown();

        boolean[] alive = new boolean[markerCount];
        Arrays.fill(alive, true);
        int aliveCount = markerCount;

        while (aliveCount > 0) {
            System.out.println("\n[main] Waiting for all alive markers to report stuck...");
            waitForStuckMarkers(stuckSignals, alive);

            printArray(array);

            int chosen = chooseMarkerToTerminate(scanner, alive, markerCount);
            markers.get(chosen).signalStop();

            try {
                markers.get(chosen).join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return;
            }

            alive[chosen] = false;
            aliveCount--;
            System.out.println("[main] Marker " + (chosen + 1) + " terminated.");

            printArray(array);

            if (aliveCount > 0) {
                System.out.println("[main] Signalling remaining markers to continue...");
                for (int i = 0; i < markerCount; i++) {
                    if (alive[i]) {
                        markers.get(i).signalContinue();
                    }
                }
            }
        }

        System.out.println("\n[main] All marker threads finished. Program complete.");
        scanner.close();
    }

    private static void waitForStuckMarkers(List<Semaphore> stuckSignals, boolean[] alive) {
        for (int i = 0; i < alive.length; i++) {
            if (!alive[i]) {
                continue;
            }
            try {
                stuckSignals.get(i).acquire();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return;
            }
        }
    }

    private static int chooseMarkerToTerminate(Scanner scanner, boolean[] alive, int markerCount) {
        while (true) {
            int markerNumber = promptInt(
                    scanner,
                    "Enter marker number to terminate (1-" + markerCount + "): ",
                    1,
                    markerCount
            );
            int markerIndex = markerNumber - 1;
            if (alive[markerIndex]) {
                return markerIndex;
            }
            System.err.println("  Marker " + markerNumber + " is already terminated. Choose another.");
        }
    }

    private static void printArray(AtomicIntegerArray array) {
        StringBuilder builder = new StringBuilder("Array: [ ");
        for (int i = 0; i < array.length(); i++) {
            builder.append(array.get(i)).append(' ');
        }
        builder.append(']');
        System.out.println(builder);
    }

    private static int promptInt(Scanner scanner, String prompt, int minValue, int maxValue) {
        while (true) {
            System.out.print(prompt);
            if (scanner.hasNextInt()) {
                int value = scanner.nextInt();
                if (value >= minValue && value <= maxValue) {
                    return value;
                }
            } else {
                scanner.next();
            }
            System.err.println("  Enter an integer between " + minValue + " and " + maxValue + ".");
        }
    }
}
