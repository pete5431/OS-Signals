# OS-Signals

Only two types of signals being sent and handled: SIGUSR1 and SIGUSR2

All handlers will increment a global received count of the respective signal type.

All signalers will increment a global sent count of the respective signal type and send randomly sends a signal.

The reporter process accepts both signals and writes a report every 10 signals.

Program 1:

Uses processes to send and handle signals:

2 processes for SIGUSR1 handling
2 processes for SIGUSR2 handling
3 processes for signal generating
1 process for reporting

Program 2:

Uses threads to send and handle signals:

2 threads for SIGUSR1 handling
2 threads for SIGUSR2 handling
3 threads for signal generating
1 thread for reporting


Both programs can be compiled using the makefile.

Both programs accept a second argument for a total signal count, which will set the limit for number of signals before program terminates.

If no argument, it will default to run for 30 seconds.

Program 1 writes into the process report file.

Program 2 writes into the threads report file.

The Results folder contains runs for conditions 30 seconds and 100000 signals.
