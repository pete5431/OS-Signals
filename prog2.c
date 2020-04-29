#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

// Used for conversion from nanoseconds to seconds.
#define BILLION 1000000000;

// Struct that contains global counts and mutex lock.
typedef struct{

	int r1_received;
	int r2_received;
	int r1_sent;
	int r2_sent;

	pthread_mutex_t counter_lock;

} Counter;

// Struct that contains variables used to calculate report statistics.
typedef struct{

	// Stores the last time a signal was received of the respective type.
	struct timespec r1_start_time;
	struct timespec r2_start_time;
	// Stores the current system time.
	struct timespec system_time;

	// Test variable. Prints how many signals reporter thread received.
	int r_count;
	// The file descriptor for the file.
	int fp;

	// The current count of signals before a report is made (once 10 signals is reached).
	int report_count;
	// The current count of SIGUSR1 signals so far in 10 signal interval.
	int r1_report_count;
	// The current count of SIGUSR2 signals so far in 10 signal interval.
	int r2_report_count;
	// The total for time interval between SIGUSR1 received.
	double r1_time_sum;
	// The total for time interval between SIGUSR2 received.
	double r2_time_sum;

} Reporter;

// Global struct variable for counter.
Counter* counter;
// Global struct variable for reporter.
Reporter* reporter;

// Global array that contains the thread ID's.
pthread_t threads[8];

// Creates all the threads one by one.
void create_threads();
// Thread that handles SIGUSR1 signal.
void* r1_handler_thread(void* arg);
// Thread that handles SIGUSR2 signal.
void* r2_handler_thread(void* arg);
// Thread that signals other threads.
void* signaler_thread(void* arg);
// Thread that writes reports to a file.
void* reporter_thread(void* arg);

// Initializes all the counter variables and allocates memory for counter.
void initialize_counter();
// Initializes all the reporter variables and allocates memory for reporter.
void initialize_reporter();
// Sleep for a random interval between 0.01 and 0.1 seconds.
void rand_sleep();
// Generate a random double from 0 to 1.
double rand_prob();

// Blocks the signal SIGUSR1.
void block_SIGUSR1();
// Unblocks the signal SIGUSR2.
void unblock_SIGUSR1();
// Blocks the signal SIGUSR2.
void block_SIGUSR2();
// Unblocks the signal SIGUSR2.
void unblock_SIGUSR2();

// Parent thread.
int main(int argc, char* argv[]){

	// Set the seed using the current time.
	srand(time(NULL));

	// Initialize global structs.
	initialize_counter();
	initialize_reporter();


	// Block SIGUSR1 and SIGUSR2. Mask inherited by threads.
	block_SIGUSR1();
	block_SIGUSR2();

	// Create all the threads.
	create_threads();

	// If a total signal count number was provided in the program command line argument.
	if(argc == 2){
		// Convert to an int.
		int total = atoi(argv[1]);
	
		// Continously check until the total amount of signals received is less than total.
		while((counter->r1_received + counter->r2_received) < total){
		}
	}
	else{
		// If no total signal count is provided, by default sleep for the specified amount of seconds.
		sleep(5);
	}	

	// Cancel all threads by requesting cancellation.
	for(int i = 0; i < 8; i++){
		pthread_cancel(threads[i]);
	}

	// Wait for thread to exit.
	for(int i = 0; i < 8; i++){
		pthread_join(threads[i], NULL);
		printf("Thead %ld has succesfully exited.\n", threads[i]);
	}

	// TEST printfs.
	printf("R1 count: %d\n", counter->r1_received);

        printf("R1 sent: %d\n", counter->r1_sent);

        printf("R2 count: %d\n", counter->r2_received);

        printf("R2 sent: %d\n", counter->r2_sent);		

	printf("Reporter count: %d\n", reporter->r_count);

	// Free global structs.
	free(counter);
	free(reporter);

	return 0;
}

// Function for thread that handles SIGUSR1 signals.
void* r1_handler_thread(void* arg){

	// Contains the set of signals to wait for.
	sigset_t signal_targets;
	// The signal that was caught.
	int accepted_signal;
	// Initialize set.
	sigemptyset(&signal_targets);
	// Add SIGUSR1 to set.
	sigaddset(&signal_targets, SIGUSR1);

	while(1){

		// Wait for SIGUSR1 to be pending before unblocking and receiving.
		sigwait(&signal_targets, &accepted_signal);

		// If the caught signal was SIGUSR1.
		if(accepted_signal == SIGUSR1){
			// Increment the count for SIGUSR1 signal.
			pthread_mutex_lock(&(counter->counter_lock));
			(counter->r1_received)++;
			pthread_mutex_unlock(&(counter->counter_lock));
		}
	}
}

// Function for thread that handles SIGUSR2 signals.
void* r2_handler_thread(void* arg){

	// Contains the set of signals to wait for.
	sigset_t signal_targets;
	// The signal that was caught.
        int accepted_signal;
	// Initialize set.
        sigemptyset(&signal_targets);
	// Add SIGUSR2 to set.
        sigaddset(&signal_targets, SIGUSR2);

	while(1){

		// Wait for SIGUSR2 to be pending before unblocking and receiving.
		sigwait(&signal_targets, &accepted_signal);

		// If the caught signal was SIGUSR2.
                if(accepted_signal == SIGUSR2){
			// Increment the count for SIGUSR2 signal.
                        pthread_mutex_lock(&(counter->counter_lock));
                        (counter->r2_received)++;
                        pthread_mutex_unlock(&(counter->counter_lock));
                }
	}
}

// Thread that generates signals to the other listening threads.
void* signaler_thread(void* arg){
	
	// Initialize the probability variable used to determine which signal to send.
	double prob = 0.0;
	// The signal to be sent.
	int signal;

	while(1){

		// Generate the random double.
		prob = rand_prob();

		// By default the probability is around 50/50 for each signal.
		if(prob < 0.5){
			signal = SIGUSR1;
		}
		else signal = SIGUSR2;

		// Send the signal to each listening thread one by one using pthread_kill.
		for(int i = 0; i < 5; i++){
			pthread_kill(threads[i], signal);
		}

		// If the signal was SIGUSR1.
		if(signal == SIGUSR1){
			// Increment the SIGUSR1 sent count.
			pthread_mutex_lock(&(counter->counter_lock));
			(counter->r1_sent)++;
			pthread_mutex_unlock(&(counter->counter_lock));
		}
		// If the signal was SIGUSR2.
		else if(signal == SIGUSR2){
			// Increment the SIGUSR2 sent count.
			pthread_mutex_lock(&(counter->counter_lock));
			(counter->r2_sent)++;
			pthread_mutex_unlock(&(counter->counter_lock));
		}
		// After one repetition sleep for a random interval.
		rand_sleep();
	}
}

// Thread that writes reports to file every 10 signals.
void* reporter_thread(void* arg){
	
	// Set of signals to wait for.
	sigset_t signal_targets;
	// The caught signal.
        int accepted_signal;
	// Initialize the set.
        sigemptyset(&signal_targets);
	// Add SIGUSR1 to set.
        sigaddset(&signal_targets, SIGUSR1);
	// Add SIGUSR2 to set.
	sigaddset(&signal_targets, SIGUSR2);

	// Stores the report message to write to file.
	char message[200];
	// The average of interval between SIGUSR1 signal reception.
	double r1_average = 0.0;
	// The average of interval between SIGUSR2 signal reception.
	double r2_average = 0.0;
	// Used to get the local time in hours minutes and seconds.
	struct tm t;
	// Used to store current time from time(NULL);
	time_t current_time;
	// The seconds that have passed for the interval.
	double seconds_passed;
	// The nanoseconds that have passed for the interval.
	double nseconds_passed;
	// The total seconds that passed in the interval.
	double time_passed;

	while(1){

		// Wait for signal. Unblock with signal is pending and receive it.
		sigwait(&signal_targets, &accepted_signal);

		// Increment the report count, writes report when it is 10.
		reporter->report_count++;
		// Test variable to print how many total signals received by reporter thread.
		reporter->r_count++;

		// Gets the current monotonic time since program started.
		clock_gettime(CLOCK_MONOTONIC, &(reporter->system_time));

		if(accepted_signal == SIGUSR1){

			// Calculate the amount of seconds passed in the interval since last SIGUSR1 reception to current SIGUSR1 reception.
			seconds_passed = (double) (reporter->system_time.tv_sec - reporter->r1_start_time.tv_sec);
                        nseconds_passed = (double) (reporter->system_time.tv_nsec - reporter->r1_start_time.tv_nsec) / BILLION;
                        reporter->r1_start_time = reporter->system_time;
                        time_passed = seconds_passed + nseconds_passed;

			// Increment the count for SIGUSR1 received during 10 signal interval.
                        reporter->r1_report_count++;
			// Add the interval to the total.
                        reporter->r1_time_sum += time_passed; 

                }
                else if(accepted_signal == SIGUSR2){
			
			// Calculate the amount of seconds passed in the interval since last SIGUSR2 reception to current SIGUSR2 reception.
                       	seconds_passed = (double) (reporter->system_time.tv_sec - reporter->r2_start_time.tv_sec);
                        nseconds_passed = (double) (reporter->system_time.tv_nsec - reporter->r2_start_time.tv_nsec) / BILLION;
                        reporter->r2_start_time = reporter->system_time;
                        time_passed = seconds_passed + nseconds_passed;

			// Increment the count for SIGUSR2 received during 10 signal interval.
                        reporter->r2_report_count++;
			// Add the interval to the total.
                        reporter->r2_time_sum += time_passed;
                }
		
		// If the report count is 10, write report and reset.
		if(reporter->report_count == 10){
			
			// Reset the report count to 0.
			reporter->report_count = 0;
	
			// Get the current local time.
			current_time = time(NULL);
                	t = *localtime(&current_time);

			// The average of the interval between SIGUSR1 reception.
			r1_average = reporter->r1_time_sum / reporter->r1_report_count;
			// The average of the interval between SIGUSR2 reception.
			r2_average = reporter->r2_time_sum / reporter->r2_report_count;
		
			// Lock to ensure data doesn't change while reporting.
			pthread_mutex_lock(&(counter->counter_lock));

			// Using sprintf to convert integers and doubles to string and stored in message.
			sprintf(message, "System Time: %d:%d:%d\nSIGUSR1 count: %d SIGUSR2 count: %d\nSIGUSR1 sent: %d SIGUSR2 sent: %d\nSIGUSR1 Average: %f SIGUSR2 Average: %f\n\n"
				, t.tm_hour, t.tm_min, t.tm_sec
				, counter->r1_received, counter->r2_received
				, counter->r1_sent, counter->r2_sent
				, r1_average, r2_average);

			// Unlock once report is done.
			pthread_mutex_unlock(&(counter->counter_lock));

			// Write the report to the file.
			write(reporter->fp, message, strlen(message));

			// Reset all the reporter variables.
			reporter->r1_report_count = 0;
			reporter->r2_report_count = 0;
			reporter->r1_time_sum = 0.0;
			reporter->r2_time_sum = 0.0;
		}
	}
}

// Sleep for a random interval.
void rand_sleep(){
	
	// Calculate a random double between 0.01 and 0.1.
	double interval = ((double) rand() / (double) RAND_MAX) * (0.09) + 0.01;

	// Use timespec struct in order to nanosleep.
	struct timespec tm;
	
	// The amount of seconds is always zero because we never reach one second.
	tm.tv_sec = 0;
	// Covert the interval seconds to nanoseconds.
	tm.tv_nsec = interval * BILLION;

	// Sleep for the corresponding nanoseconds.
	nanosleep(&tm, NULL);
}

// Simple random double generator from 0 to 1.
double rand_prob(){

	return ((double) rand() / (double) RAND_MAX);
}

// Initializes the global counter variables.
void initialize_counter(){
	
	// Allocate memory for the counter.
	counter = (Counter*) malloc(sizeof(Counter));

	if(counter == NULL){
		printf("Memory Allocation Error.\n");
		exit(1);
	}

        counter->r1_received = 0;
        counter->r2_received = 0;
        counter->r1_sent = 0;
        counter->r2_sent = 0;

	// Initialize the lock variable.
	pthread_mutex_init(&(counter->counter_lock), NULL);
}

// Initializes the global reporter variables.
void initialize_reporter(){

	reporter = (Reporter*) malloc(sizeof(Reporter));

	if(reporter == NULL){
		printf("Memory Allocation Error.\n");
		exit(1);
	}

        clock_gettime(CLOCK_MONOTONIC, &(reporter->r1_start_time));

        clock_gettime(CLOCK_MONOTONIC, &(reporter->r2_start_time));

        reporter->fp = open("thread_reports.txt", O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);

        reporter->r_count = 0;

        reporter->report_count = 0;

        reporter->r1_time_sum = 0.0;

        reporter->r2_time_sum = 0.0;

        reporter->r1_report_count = 0;

        reporter->r2_report_count = 0;
}

// Creates the threads.
void create_threads(){

	if(pthread_create(&threads[0], NULL, reporter_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }

        if(pthread_create(&threads[1], NULL, r1_handler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }               

        if(pthread_create(&threads[2], NULL, r1_handler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }

	if(pthread_create(&threads[3], NULL, r2_handler_thread, NULL) != 0){
		printf("Thread create failed.\n");
		exit(1);
	}

	if(pthread_create(&threads[4], NULL, r2_handler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }

	if(pthread_create(&threads[5], NULL, signaler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }

	if(pthread_create(&threads[6], NULL, signaler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }

	if(pthread_create(&threads[7], NULL, signaler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
}

// Blocks the signal SIGUSR1 in the calling thread.
void block_SIGUSR1(){
	// Set of signals to block.
        sigset_t set;
	// Initialize the set.
        sigemptyset(&set);
	// Add SIGUSR1 to the set.
        sigaddset(&set, SIGUSR1);
	// Blocks the signals in the set in the current mask.
        pthread_sigmask(SIG_BLOCK, &set, NULL);
}

// Unblocks the signal SIGUSR1 in the calling thread.
void unblock_SIGUSR1(){
	// Set of signals to unblock.
        sigset_t set;
	// Initialize the set.
        sigemptyset(&set);	
	// Add SIGUSR1 to the set. 
        sigaddset(&set, SIGUSR1);
	// Blocks the signals in the set in the current mask.
        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}

// Blocks the signal SIGUSR2 in the calling thread.
void block_SIGUSR2(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR2);

        pthread_sigmask(SIG_BLOCK, &set, NULL);

}

// Unblocks the signal SIGUSR2 in the calling thread.
void unblock_SIGUSR2(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR2);

        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}
