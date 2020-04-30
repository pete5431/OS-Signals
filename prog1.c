#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

// Two signals used for communication: SIGUSR1 and SIGUSR2
// Shared signal generated/sent counter for both.

#define BILLION 1000000000;

// Contains the global counters for SIGUSR1 and SIGUSR2.
typedef struct{

	int r1_received;
	int r2_received;
	int r1_sent;
	int r2_sent;

	pthread_mutex_t counter_lock;

} Counter;

// Contains the variables used to calculate statistics for reports.
typedef struct{

	// Records the time of last signal received of the respective time.
	struct timespec r1_start_time;
	struct timespec r2_start_time;
	// The current system time.
	struct timespec system_time;

	// For testing purposes.
	int r_count;
	// Flag for whether to write a report.
	int write_report;

	// The file descriptor for the report file.
	int fp;
	// The total report count that resets when it hits 10.
	int report_count;
	// The total SIGUSR1 signal received during 10 signal interval.
	int r1_report_count;
	// The total SIGUSR2 signal received during 10 signal interval.
	int r2_report_count;
	// The average of the interval between SIGUSR1 received.
	double r1_time_sum;
	// The average of the interval between SIGUSR2 received.
	double r2_time_sum;

} Reporter;

// The global structs that will be in shared memory.
Counter* counter;
Reporter* reporter;

// The id used to access the repective shared memories.
int shm_id_counter;
int shm_id_reporter;

// Process ids.
pid_t processes[8];
// Function to create processes.
void create_processes();

// Function for the child process that handles SIGUSR1.
void r1_handler_process();
// Function for the child process that handles SIGUSR2.
void r2_handler_process();
// Function that for signal generator process.
void signaler_process();
// Function for reporter process.
void reporter_process();

// Handler for handler processes and signaler process.
void signal_handler(int);
// Handler for reporter process.
void signal_report_handler(int);

// Initialize the global counter struct.
void initialize_counter();
// Initialize the global reporter struct.
void initialize_reporter();
// Sleep for random interval.
void rand_sleep();
// Generate a random probability.
double rand_prob();

// Functions for blocking and unblocking.
void block_SIGUSR1();
void unblock_SIGUSR1();
void block_SIGUSR2();
void unblock_SIGUSR2();
void block_SIGTERM();
void unblock_SIGTERM();

int main(int argc, char* argv[]){

	// Set the seed as the current time.
	srand(time(NULL));

	// Initialize the shared global counter and reporter.
	initialize_counter();
	initialize_reporter();

	// Block SIGUSR1, SIGUSR2 and SIGTERM.
	block_SIGUSR1();
	block_SIGUSR2();
	block_SIGTERM();

	// Attach shared memory for counter.
	counter = (Counter*) shmat(shm_id_counter, 0, 0);

	// Create processes.
	create_processes();

	// If argument for a signal count present.
	if(argc == 2){

		int total = atoi(argv[1]);
		// Loop until count is not less than total.
		while((counter->r1_received + counter->r2_received) < total){	
		}
	}else{
		// Else just sleep for specified time.
		sleep(30);
	}
	
	// Send SIGTERM to all processes to end all processes.
	for(int i = 0; i < 8; i++){
		kill(processes[i], SIGTERM);
	}
	
	int i = 0;
	int status = 0;

	// Wait for all processes to end.
	while(i != 8){
		waitpid(processes[i], &status, 0);
		i++;
	}

	// Test prints.
	printf("R1 count: %d\n", counter->r1_received);

	printf("R1 sent: %d\n", counter->r1_sent);

	printf("R2 count: %d\n", counter->r2_received);
	
	printf("R2 sent: %d\n", counter->r2_sent);

	// Detach and destroy shared memory.
	shmdt(counter);
	shmdt(reporter);
	shmctl(shm_id_counter, IPC_RMID, NULL);
	shmctl(shm_id_reporter, IPC_RMID, NULL);

	return 0;
}

void r1_handler_process(){

	// Establish signal handler for SIGUSR1 and SIGTERM.
	signal(SIGUSR1, signal_handler);
	signal(SIGTERM, signal_handler);

	// Set up a wait mask for sigsuspend.
	sigset_t wait_mask;
	// Fill the set.
	sigfillset(&wait_mask);
	// Delete SIGUSR1.
	sigdelset(&wait_mask, SIGUSR1);
	// Delete SIGTERM.
	sigdelset(&wait_mask, SIGTERM);

	while(1){
		// Now suspends until SIGUSR1 or SIGTERM is received.
		sigsuspend(&wait_mask);
	}
}

void r2_handler_process(){

	// Establish signal handler for SIGUSR2 and SIGTERM.
	signal(SIGUSR2, signal_handler);
	signal(SIGTERM, signal_handler);

	// Set up a wait mask for sigsuspend.
	sigset_t wait_mask;
        sigfillset(&wait_mask);
	sigdelset(&wait_mask, SIGUSR2);
	sigdelset(&wait_mask, SIGTERM);

	while(1){
		// Now suspends until SIGUSR2 or SIGTERM is received.
		sigsuspend(&wait_mask);
	}
}

// Signal generator process.
void signaler_process(){

	// Establish SIGTERM handler. Used to terminate process.
	signal(SIGTERM, signal_handler);
	// Unblock SIGTERM.
	unblock_SIGTERM();

	double prob = 0.0;
	int signal;

	while(1){

		// Generate rand probability.
		prob = rand_prob();

		// Determine which signal base off prob.
		if(prob < 0.5){
			signal = SIGUSR1;
		}else{
			signal = SIGUSR2;
		}

		// Send the signal to every process in processes.
		for(int i = 0; i < 5; i++){
			// Invoke kill system call to request kernel to send signal to the following process.
			kill(processes[i], signal);
		}

		// Increment the corresponding count that is mutually exclusive.
		if(signal == SIGUSR1){

			pthread_mutex_lock(&(counter->counter_lock));
		
			(counter->r1_sent)++;

			pthread_mutex_unlock(&(counter->counter_lock));
		}
		else if(signal == SIGUSR2){

			pthread_mutex_lock(&(counter->counter_lock));

			(counter->r2_sent)++;

			pthread_mutex_unlock(&(counter->counter_lock));
		}
		// After each repetition sleep for random interval.
		rand_sleep();
	}
}

// Process for reporter.
void reporter_process(){

	// Establish signal handlers for SIGTERM, SIGUSR1, and SIGUSR2.
	signal(SIGTERM, signal_report_handler);
	signal(SIGUSR1, signal_report_handler);
	signal(SIGUSR2, signal_report_handler);

	// Create mask for sigsuspend.
	sigset_t wait_mask;
	sigfillset(&wait_mask);
	sigdelset(&wait_mask, SIGUSR1);
	sigdelset(&wait_mask, SIGUSR2);
	sigdelset(&wait_mask, SIGTERM);

	// Stores the message to write to report file.
	char message[200];
	// The average of the interval between SIGUSR1 reception.
	double r1_average = 0.0;
	// The average of the interval between SIGUSR2 reception.
	double r2_average = 0.0;
	// Used to obtain local time in easier read format.
	struct tm t;
	time_t current_time;

	while(1){
	
		// Suspend until SIGUSR1, SIGUSR2, or SIGTERM is received.
		sigsuspend(&wait_mask);

		// If the flag for write_report is true.
		if(reporter->write_report){
		
			// Get the local time.
			current_time = time(NULL);
			t = *localtime(&current_time);	

			// Calculate average.
			r1_average = reporter->r1_time_sum / reporter->r1_report_count; 
			// Calculate average.
			r2_average = reporter->r2_time_sum / reporter->r2_report_count;

			pthread_mutex_lock(&(counter->counter_lock));
			// Use sprintf to convert integer and double to string.
			sprintf(message, "System Time: %d:%d:%d\nSIGUSR1 count: %d SIGUSR2 count: %d\nSIGUSR1 sent: %d  SIGUSR2 sent: %d\nSIGUSR1 Average: %f SIGUSR2 Average: %f\n\n"
				, t.tm_hour, t.tm_min, t.tm_sec
				, counter->r1_received, counter->r2_received
				, counter->r1_sent, counter->r2_sent
				, r1_average, r2_average);
	
			pthread_mutex_unlock(&(counter->counter_lock));

			// Write the message to report file.
			write(reporter->fp, message, strlen(message));

			// Reset the values.
			reporter->r1_report_count = 0;
			reporter->r2_report_count = 0;
			reporter->r1_time_sum = 0.0;
			reporter->r2_time_sum = 0.0;
		}
		// Set the flag to false again.
		reporter->write_report = 0;
	}
}

// Random sleep for random time between 0.01 and 0.1 seconds.
void rand_sleep(){

	double interval = ((double)rand() / (double)RAND_MAX) * (0.09) + 0.01;

	struct timespec tm;

        tm.tv_sec = 0;

	// Convert to nanoseconds.
       	tm.tv_nsec = interval * BILLION;
	// Sleep for the corresponding nanoseconds.
        nanosleep(&tm, NULL);
}

// Generic rand double generator for between 0 and 1.
double rand_prob(){
	return ((double) rand() / (double) RAND_MAX);
}

// The signal handler for handler and signal generator processes.
void signal_handler(int sig){

	// Increments accordingly to the signal received. If SIGTERM, detach and exit.
	if(sig == SIGUSR1){

		pthread_mutex_lock(&(counter->counter_lock));

       		(counter->r1_received)++;

		pthread_mutex_unlock(&(counter->counter_lock));

	}
	else if(sig == SIGUSR2){

        	pthread_mutex_lock(&(counter->counter_lock));

        	(counter->r2_received)++;

        	pthread_mutex_unlock(&(counter->counter_lock));
     
	}
	else if(sig == SIGTERM){
		shmdt(counter);
		exit(EXIT_SUCCESS);
	}
}

// Signal handler for reporter process.
void signal_report_handler(int sig){

	if(sig == SIGUSR1){
	
		(reporter->report_count)++;

		(reporter->r_count)++;

		clock_gettime(CLOCK_MONOTONIC, &(reporter->system_time));

		// If reporter count is 10, set the flag.
		if(reporter->report_count == 10){

                        reporter->report_count = 0;

                        reporter->write_report = 1;
                }

		// Calculate interval between last SIGUSR1 reception.
		double seconds_passed;
                double nseconds_passed;
                double time_passed;

		seconds_passed = (double) (reporter->system_time.tv_sec - reporter->r1_start_time.tv_sec);
		nseconds_passed = (double) (reporter->system_time.tv_nsec - reporter->r1_start_time.tv_nsec) / BILLION;

		reporter->r1_start_time = reporter->system_time;
	
		time_passed = seconds_passed + nseconds_passed;

		reporter->r1_report_count++;
		reporter->r1_time_sum += time_passed;
	}
	else if(sig == SIGUSR2){
		
		(reporter->report_count)++;
                
                (reporter->r_count)++;
                
                clock_gettime(CLOCK_MONOTONIC, &(reporter->system_time));
                
		// If reporter count is 10, set the flag.
                if(reporter->report_count == 10){
                
                        reporter->report_count = 0;
                        
                        reporter->write_report = 1;
                }       

		double seconds_passed;
                double nseconds_passed;
                double time_passed;
                
                seconds_passed = (double) (reporter->system_time.tv_sec - reporter->r2_start_time.tv_sec);
                nseconds_passed = (double) (reporter->system_time.tv_nsec - reporter->r2_start_time.tv_nsec) / BILLION;
                        
                reporter->r2_start_time = reporter->system_time;
                        
                time_passed = seconds_passed + nseconds_passed;
                        
                reporter->r2_report_count++;
                reporter->r2_time_sum += time_passed;                     
	}
	else if(sig == SIGTERM){
		// If signal is sigterm, detach from all shared memory, report the report count, and exit.
		shmdt(counter);
		printf("Reporter Count: %d\n", reporter->r_count);
		close(reporter->fp);
		shmdt(reporter);
		exit(EXIT_SUCCESS);
	}
}

void initialize_counter(){

	// Using the shm functions to create a shared memory that contains one SigusCount struct.
        shm_id_counter = shmget(IPC_PRIVATE, sizeof(Counter), IPC_CREAT | 0666);

        counter = (Counter*) shmat(shm_id_counter, 0, 0);

        counter->r1_received = 0;
        counter->r2_received = 0;
        counter->r1_sent = 0;
        counter->r2_sent = 0;

        // Make mutex shared across processes using the following share attribute.
        pthread_mutexattr_t shared_attr;
        pthread_mutexattr_init(&(shared_attr));
        pthread_mutexattr_setpshared(&(shared_attr), PTHREAD_PROCESS_SHARED);

	pthread_mutex_init(&(counter->counter_lock), &(shared_attr));

	pthread_mutexattr_destroy(&shared_attr);

        // Detach the shared memory.
        shmdt(counter);
}

void initialize_reporter(){

	// Initialize and allocate memory for reporter.

	shm_id_reporter = shmget(IPC_PRIVATE, sizeof(Reporter), IPC_CREAT | 0666);

	reporter = (Reporter*) shmat(shm_id_reporter, 0, 0);

	clock_gettime(CLOCK_MONOTONIC, &(reporter->r1_start_time));

	clock_gettime(CLOCK_MONOTONIC, &(reporter->r2_start_time));

        reporter->fp = open("process_reports.txt", O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);

	reporter->r_count = 0;

	reporter->report_count = 0;

	reporter->write_report = 0;

	reporter->r1_time_sum = 0.0;
	
	reporter->r2_time_sum = 0.0;

	reporter->r1_report_count = 0;

	reporter->r2_report_count = 0;

	shmdt(reporter);
}

// Create the processes.
void create_processes(){

	// Basically locks each child process into their respective functions.

	for(int i = 0; i < 8; i++){

		processes[i] = fork();

		if(processes[i] < 0){
			printf("Error: Forking Error\n");
			exit(1);
		}
		else if(processes[i] == 0){
			if(i == 0){	
				reporter = (Reporter*) shmat(shm_id_reporter, 0, 0);
				reporter_process();
				shmdt(reporter);
				exit(1);
			}
			else if(i == 1){
				r1_handler_process();
				exit(1);
			}
			else if(i == 2){
				r1_handler_process();
				exit(1);
			}
			else if(i == 3){
				r2_handler_process();
				exit(1);
			}
			else if(i == 4){
				r2_handler_process();
				exit(1);
			}
			else if(i == 5){
				signaler_process();
				exit(1);
			}
			else if(i == 6){
				signaler_process();
				exit(1);
			}
			else if(i == 7){
				signaler_process();
				exit(1);
			}

		}
	}
} 

// Below are generic functions for blocking and unblocking a signal.

void block_SIGUSR1(){

        sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR1);

        sigprocmask(SIG_BLOCK, &set, NULL);
}

void unblock_SIGUSR1(){

	sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR1);

        sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void block_SIGUSR2(){

	sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR2);

        sigprocmask(SIG_BLOCK, &set, NULL);

}

void unblock_SIGUSR2(){

	sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGUSR2);

        sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void block_SIGTERM(){

	sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGTERM);

        sigprocmask(SIG_BLOCK, &set, NULL);

}

void unblock_SIGTERM(){

	sigset_t set;

        sigemptyset(&set);

        sigaddset(&set, SIGTERM);

        sigprocmask(SIG_UNBLOCK, &set, NULL);
}
