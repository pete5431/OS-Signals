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

	pthread_mutex_t r1_received_lock;
	pthread_mutex_t r1_sent_lock;
	pthread_mutex_t r2_received_lock;
	pthread_mutex_t r2_sent_lock;

} Counter;

// Contains the variables used to calculate statistics for reports.
typedef struct{

	struct timespec r1_start_time;
	struct timespec r2_start_time;
	struct timespec system_time;

	int r_count;
	int write_report;

	int fp;
	int report_count;
	int r1_report_count;
	int r2_report_count;
	double r1_time_sum;
	double r2_time_sum;

} Reporter;

// The global structs that will be in shared memory.
Counter* counter;
Reporter* reporter;

// The id used to access the repective shared memories.
int shm_id_counter;
int shm_id_reporter;

pid_t* create_handler_processes(int, int);
pid_t* create_signaler_processes(int);

void r1_handler_process();
void r2_handler_process();
void signaler_process();
void reporter_process();

void signal_handler(int);
void signal_report_handler(int);

void initialize_counter();
void initialize_reporter();
void rand_sleep();
double rand_prob();

void block_SIGUSR1();
void unblock_SIGUSR1();
void block_SIGUSR2();
void unblock_SIGUSR2();
void block_SIGTERM();
void unblock_SIGTERM();

int main(int argc, char* argv[]){

	srand(time(NULL));

	initialize_counter();
	initialize_reporter();

	block_SIGUSR1();
	block_SIGUSR2();
	block_SIGTERM();

	counter = (Counter*) shmat(shm_id_counter, 0, 0);

	pid_t* handler_processes = create_handler_processes(2,2);

	reporter = (Reporter*) shmat(shm_id_reporter, 0, 0);

	pid_t report_process = fork();
	if(reporter_process < 0){
		printf("Forking Error\n");
		exit(1);
	}
	else if(report_process == 0){
		reporter_process();
		exit(1);
	}

	shmdt(reporter);

	pid_t* signaler_processes = create_signaler_processes(3);

	if(argc == 2){

		int total = atoi(argv[1]);

		while((counter->r1_received + counter->r2_received) < total){	

		}

		killpg(0, SIGTERM);

	}else{
		sleep(30);

		killpg(0, SIGTERM);
	}
	
	int i = 0;
	int status = 0;

        waitpid(report_process, &status, 0);
        printf("Reporter Process %d exited with status %d\n", report_process, status);

	while(i <= 2){

                waitpid(signaler_processes[i], &status, 0);
                printf("Signaler Process %d exited with status %d\n", signaler_processes[i], status);
                i++;
        }

	i = 0;

	while(i <= 3){
		
		waitpid(handler_processes[i], &status, 0);
		printf("Handler Process %d exited with status %d\n", handler_processes[i], status);
		i++;
	}

	printf("R1 count: %d\n", counter->r1_received);

	printf("R1 sent: %d\n", counter->r1_sent);

	printf("R2 count: %d\n", counter->r2_received);
	
	printf("R2 sent: %d\n", counter->r2_sent);

	shmdt(counter);
	shmdt(reporter);
	free(handler_processes);
	free(signaler_processes);
	shmctl(shm_id_counter, IPC_RMID, NULL);
	shmctl(shm_id_reporter, IPC_RMID, NULL);

	return 0;
}

void r1_handler_process(){

	signal(SIGUSR1, signal_handler);
	signal(SIGTERM, signal_handler);

	unblock_SIGUSR1();
	unblock_SIGTERM();

	while(1){

		pause();
	}
}

void r2_handler_process(){

	signal(SIGUSR2, signal_handler);
	signal(SIGTERM, signal_handler);

	unblock_SIGUSR2();
	unblock_SIGTERM();

	while(1){

		pause();
	}
}

void signaler_process(){

	signal(SIGTERM, signal_handler);
	
	unblock_SIGTERM();

	double prob = 0.0;

	while(1){

		prob = rand_prob();

		int signal;

		if(prob < 0.5){
			signal = SIGUSR1;
		}else{
			signal = SIGUSR2;
		}

		killpg(0, signal);

		if(signal == SIGUSR1){

			pthread_mutex_lock(&(counter->r1_sent_lock));
		
			(counter->r1_sent)++;

			pthread_mutex_unlock(&(counter->r1_sent_lock));
		}
		else{

			pthread_mutex_lock(&(counter->r2_sent_lock));

			(counter->r2_sent)++;

			pthread_mutex_unlock(&(counter->r2_sent_lock));
		}
		
	
		rand_sleep();
		// Invoke kill system call to request kernel to send signal SIGUSR1 to processes in this group.
	}
}

void reporter_process(){

	signal(SIGTERM, signal_report_handler);
	signal(SIGUSR1, signal_report_handler);
	signal(SIGUSR2, signal_report_handler);

	unblock_SIGUSR1();
	unblock_SIGUSR2();
	unblock_SIGTERM();

	char message[100];
	double r1_average = 0.0;
	double r2_average = 0.0;
	struct tm t;
	time_t current_time;

	while(1){

		pause();

		if(reporter->write_report){

			current_time = time(NULL);

			t = *localtime(&current_time);	

			r1_average = reporter->r1_time_sum / reporter->r1_report_count; 

			r2_average = reporter->r2_time_sum / reporter->r2_report_count;

			pthread_mutex_lock(&(counter->r1_received_lock));
			pthread_mutex_lock(&(counter->r2_received_lock));
			pthread_mutex_lock(&(counter->r1_sent_lock));
			pthread_mutex_lock(&(counter->r2_sent_lock));

			sprintf(message, "System Time: %d:%d:%d\nSIGUSR1 count: %d SIGUSR2 count: %d\nSIGUSR1 sent: %d  SIGUSR2 sent: %d\nSIGUSR1 Average: %f SIGUSR2 Average: %f\n\n"
				, t.tm_hour, t.tm_min, t.tm_sec
				, counter->r1_received, counter->r2_received
				, counter->r1_sent, counter->r2_sent
				, r1_average, r2_average);
	
			pthread_mutex_unlock(&(counter->r1_received_lock));	
			pthread_mutex_unlock(&(counter->r2_received_lock)); 
			pthread_mutex_unlock(&(counter->r1_sent_lock)); 
			pthread_mutex_unlock(&(counter->r2_sent_lock)); 

			write(reporter->fp, message, strlen(message));

			reporter->r1_report_count = 0;
			reporter->r2_report_count = 0;
			reporter->r1_time_sum = 0.0;
			reporter->r2_time_sum = 0.0;
		}

		reporter->write_report = 0;
	}
}

void rand_sleep(){

	double interval = ((double)rand() / (double)RAND_MAX) * (0.09) + 0.01;

	struct timespec tm;

        tm.tv_sec = 0;

       	tm.tv_nsec = interval * BILLION;

        nanosleep(&tm, NULL);
}

double rand_prob(){
	return ((double) rand() / (double) RAND_MAX);
}

void signal_handler(int sig){

	if(sig == SIGUSR1){

		pthread_mutex_lock(&(counter->r1_received_lock));

       		(counter->r1_received)++;

		pthread_mutex_unlock(&(counter->r1_received_lock));

	}
	else if(sig == SIGUSR2){

        	pthread_mutex_lock(&(counter->r2_received_lock));

        	(counter->r2_received)++;

        	pthread_mutex_unlock(&(counter->r2_received_lock));
     
	}
	else if(sig == SIGTERM){
		shmdt(counter);
		exit(EXIT_SUCCESS);
	}
}

void signal_report_handler(int sig){

	if(sig == SIGUSR1 | sig == SIGUSR2){
		
		(reporter->report_count)++;

		(reporter->r_count)++;
	
		clock_gettime(CLOCK_MONOTONIC, &(reporter->system_time));

		if(reporter->report_count == 10){

                        reporter->report_count = 0;

                        reporter->write_report = 1;
                }

		double seconds_passed;
                double nseconds_passed;
                double time_passed;

		if(sig == SIGUSR1){

			seconds_passed = (double) (reporter->system_time.tv_sec - reporter->r1_start_time.tv_sec);
			nseconds_passed = (double) (reporter->system_time.tv_nsec - reporter->r1_start_time.tv_nsec) / BILLION;

			reporter->r1_start_time = reporter->system_time;
	
			time_passed = seconds_passed + nseconds_passed;

			reporter->r1_report_count++;
			reporter->r1_time_sum += time_passed;
		}
		else if(sig == SIGUSR2){

			seconds_passed = (double) (reporter->system_time.tv_sec - reporter->r2_start_time.tv_sec);
			nseconds_passed = (double) (reporter->system_time.tv_nsec - reporter->r2_start_time.tv_nsec) / BILLION;

			reporter->r2_start_time = reporter->system_time;

			time_passed = seconds_passed + nseconds_passed;

			reporter->r2_report_count++;
			reporter->r2_time_sum += time_passed;
		}
	}
	else if(sig == SIGTERM){
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

        pthread_mutex_init(&(counter->r1_received_lock), &(shared_attr));
        pthread_mutex_init(&(counter->r1_sent_lock), &(shared_attr));
	pthread_mutex_init(&(counter->r2_received_lock), &(shared_attr));
	pthread_mutex_init(&(counter->r2_sent_lock), &(shared_attr));

	pthread_mutexattr_destroy(&shared_attr);

        // Detach the shared memory.
        shmdt(counter);
}

void initialize_reporter(){

	shm_id_reporter = shmget(IPC_PRIVATE, sizeof(Reporter), IPC_CREAT | 0666);

	reporter = (Reporter*) shmat(shm_id_reporter, 0, 0);

	clock_gettime(CLOCK_MONOTONIC, &(reporter->r1_start_time));

	clock_gettime(CLOCK_MONOTONIC, &(reporter->r2_start_time));

        reporter->fp = open("reports.txt", O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);

	reporter->r_count = 0;

	reporter->report_count = 0;

	reporter->r1_time_sum = 0.0;
	
	reporter->r2_time_sum = 0.0;

	reporter->r1_report_count = 0;

	reporter->r2_report_count = 0;

	shmdt(reporter);
}

pid_t* create_handler_processes(int r1_num, int r2_num){

	pid_t* handler_processes = malloc((r1_num + r2_num) * sizeof(pid_t));

	if(handler_processes == NULL){
		printf("Error: Memory Allocation Failed.\n");
		exit(1);
	}

	int i = 0;

	for(; i < r1_num; i++){
		handler_processes[i] = fork();
		if(handler_processes[i] < 0){
			printf("Forking Error\n");
			exit(1);
		}
		else if(handler_processes[i] == 0){
			
			r1_handler_process();
			
			exit(1);
		} 
	}
	for(; i < r1_num + r2_num; i++){
		handler_processes[i] = fork();
		if(handler_processes[i] < 0){
			printf("Forking Error\n");
			exit(1);
		}
		else if(handler_processes[i] == 0){
			r2_handler_process();
			exit(1);
		}
	}

	return handler_processes;
}

pid_t* create_signaler_processes(int num){

	pid_t* signaler_processes = malloc((num) * sizeof(pid_t));

	if(signaler_processes == NULL){
		printf("Error: Memory Allocation Failed.\n");
		exit(1);
	}
	
	for(int i = 0; i < num; i++){
		signaler_processes[i] = fork();
		if(signaler_processes[i] < 0){
			printf("Forking Error\n");
			exit(1);
		}
		else if(signaler_processes[i] == 0){
			signaler_process();
			exit(1);
		}
	}

	return signaler_processes;
} 

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
