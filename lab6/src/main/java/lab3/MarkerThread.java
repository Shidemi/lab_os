package lab3;

import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicIntegerArray;

/**
 * MarkerThread tries to write its marker index into random free array cells.
 * When it reaches an occupied cell, it reports the blocked state to main and
 * waits for a continue or stop command.
 */
public class MarkerThread extends Thread {

    private static final int SLEEP_MS = 5;

    private final int markerIndex;
    private final AtomicIntegerArray array;
    private final CountDownLatch startLatch;
    private final Semaphore stuckSignal;
    private final Semaphore commandSignal;

    private volatile boolean stopRequested = false;
    private volatile boolean continueRequested = false;

    public MarkerThread(
            int markerIndex,
            AtomicIntegerArray array,
            CountDownLatch startLatch,
            Semaphore stuckSignal,
            Semaphore commandSignal
    ) {
        super("marker-" + markerIndex);
        this.markerIndex = markerIndex;
        this.array = array;
        this.startLatch = startLatch;
        this.stuckSignal = stuckSignal;
        this.commandSignal = commandSignal;
    }

    public void signalStop() {
        stopRequested = true;
        commandSignal.release();
    }

    public void signalContinue() {
        continueRequested = true;
        commandSignal.release();
    }

    @Override
    public void run() {
        try {
            startLatch.await();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return;
        }

        Random random = new Random(markerIndex);

        while (true) {
            int index = random.nextInt(array.length());

            boolean marked;
            try {
                Thread.sleep(SLEEP_MS);
                marked = array.compareAndSet(index, 0, markerIndex);
                Thread.sleep(SLEEP_MS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                clearMarkedCells();
                return;
            }

            if (marked) {
                continue;
            }

            int markedCount = countMarkedCells();
            System.out.printf(
                    "[marker %d] stuck at index=%d, marked=%d cell(s)%n",
                    markerIndex,
                    index,
                    markedCount
            );

            stuckSignal.release();

            waitForCommand();

            if (stopRequested) {
                clearMarkedCells();
                System.out.printf("[marker %d] terminated, cells cleared.%n", markerIndex);
                return;
            }

            continueRequested = false;
        }
    }

    private void waitForCommand() {
        while (!stopRequested && !continueRequested) {
            try {
                commandSignal.acquire();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                stopRequested = true;
                return;
            }
        }
    }

    private int countMarkedCells() {
        int count = 0;
        for (int i = 0; i < array.length(); i++) {
            if (array.get(i) == markerIndex) {
                count++;
            }
        }
        return count;
    }

    private void clearMarkedCells() {
        for (int i = 0; i < array.length(); i++) {
            array.compareAndSet(i, markerIndex, 0);
        }
    }
}
