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

// Two signals used for communication: SIGUSR1 and SIGUSR2
// Shared signal generated/sent counter for both.

typedef struct{

	int r1_received;
	int r2_received;
	int r1_sent;
	int r2_sent;

	//pthread_mutex_t r1_received_lock;
	pthread_mutex_t r1_lock;
	//pthread_mutex_t r2_received_lock;
	pthread_mutex_t r2_lock;

} Counter;

typedef struct{

	struct timespec start_time;
	struct timespec end_time;
	FILE* fp;
	int report_count;
	int r_count;
	int r1_report_count;
	int r2_report_count;
	double r1_time_sum;
	double r2_time_sum;
	double previous_r1_time;
	double previous_r2_time;

} Reporter;

Counter* counter;
Reporter* reporter;

int shm_id;

char* r1_msg = "SIGUSR1 received\n";
char* r2_msg = "SIGUSR2 received\n";

pid_t* create_handler_processes(int, int);
pid_t* create_signaler_processes(int);

void r1_handler_process();
void r2_handler_process();
void signaler_process();
void reporter_process();

void signal_handler(int);
void signal_report_handler(int);

void initialize_globals();
double rand_interval();
double rand_prob();

void block_SIGUSR1();
void unblock_SIGUSR1();
void block_SIGUSR2();
void unblock_SIGUSR2();
void block_SIGTERM();
void unblock_SIGTERM();

int main(int argc, char* argv[]){

	srand(time(NULL));

	initialize_globals();

	block_SIGUSR1();
	block_SIGUSR2();
	block_SIGTERM();

	counter = (Counter*) shmat(shm_id, 0, 0);

	pid_t* handler_processes = create_handler_processes(2,2);

	pid_t report_process = fork();
	if(reporter_process < 0){
		printf("Forking Error\n");
		exit(1);
	}
	else if(report_process == 0){
		reporter_process();
		exit(1);
	}

	pid_t* signaler_processes = create_signaler_processes(3);

	sleep(5);

	killpg(0, SIGTERM);

	int i = 0;
	int status = 0;

	//kill(report_process, SIGTERM);
        waitpid(report_process, &status, 0);
        printf("Reporter Process %d exited with status %d\n", report_process, status);

	while(i <= 3){
		
		//kill(handler_processes[i], SIGTERM);

		waitpid(handler_processes[i], &status, 0);
		printf("Handler Process %d exited with status %d\n", handler_processes[i], status);
		i++;
	}

	i = 0;

	while(i <= 2){

		//kill(signaler_processes[i], SIGTERM);
	
		waitpid(signaler_processes[i], &status, 0);
		printf("Signaler Process %d exited with status %d\n", signaler_processes[i], status);
		i++;
	}

	printf("R1 count: %d\n", counter->r1_received);

	printf("R1 sent: %d\n", counter->r1_sent);

	printf("R2 count: %d\n", counter->r2_received);
	
	printf("R2 sent: %d\n", counter->r2_sent);

	shmdt(counter);
	free(handler_processes);
	free(signaler_processes);
	shmctl(shm_id, IPC_RMID, NULL);

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

	double interval = 0.0;

	double prob = 0.0;

	struct timespec tm;

	while(1){

		interval = rand_interval();

		prob = rand_prob();

		tm.tv_sec = 0;

		tm.tv_nsec = interval * 1000000000;

		printf("Rand Interval: %ld\n", tm.tv_nsec);

		nanosleep(&tm, NULL);

		if(prob < 0.50){
			killpg(0, SIGUSR1);

			pthread_mutex_lock(&(counter->r1_lock));
		
			(counter->r1_sent)++;

			pthread_mutex_unlock(&(counter->r1_lock));

		}else{
			killpg(0, SIGUSR2);

			pthread_mutex_lock(&(counter->r1_lock));

			(counter->r2_sent)++;

			pthread_mutex_unlock(&(counter->r1_lock));

		}
		// Invoke kill system call to request kernel to send signal SIGUSR1 to processes in this group.
	}
}

void reporter_process(){

	signal(SIGTERM, signal_report_handler);
	signal(SIGUSR1, signal_report_handler);
	signal(SIGUSR2, signal_report_handler);

	reporter = (Reporter*) malloc(sizeof(Reporter));

	clock_gettime(CLOCK_MONOTONIC, &(reporter->start_time));

	reporter->r_count = 0;

	unblock_SIGUSR1();
	unblock_SIGUSR2();
	unblock_SIGTERM();

	while(1){

		pause();
	}
}

double rand_interval(){
	return ((double) rand() / RAND_MAX) * (0.09) + 0.01;
}

double rand_prob(){
	return ((double) rand() / (double) RAND_MAX);
}

void signal_handler(int sig){

	if(sig == SIGUSR1){
		//write(STDOUT_FILENO, r1_msg, strlen(r1_msg));

		pthread_mutex_lock(&(counter->r1_lock));

       		(counter->r1_received)++;

		pthread_mutex_unlock(&(counter->r1_lock));

	}
	else if(sig == SIGUSR2){
		//write(STDOUT_FILENO, r2_msg, strlen(r2_msg));

        	pthread_mutex_lock(&(counter->r1_lock));

        	(counter->r2_received)++;

        	pthread_mutex_unlock(&(counter->r1_lock));
     
	}
	else if(sig == SIGTERM){
		shmdt(counter);
		exit(EXIT_SUCCESS);
	}
}

void signal_report_handler(int sig){

	if(sig == SIGUSR1){
		//write(STDOUT_FILENO, r1_msg, strlen(r1_msg));
		//(reporter->report_count)++;

		//pthread_mutex_lock(&(counter->r1_lock));

		(reporter->r_count)++;

		//pthread_mutex_unlock(&(counter->r1_lock));

		/*		
		if(reporter->report_count == 10){
			reporter->report_count = 0;

			


			

			char message[12];
			clock_gettime(CLOCK_MONOTONIC, &(reporter->end_time));

			double time_passed = (double) (reporter->end_time.tv_sec - reporter->start_time.tv_sec) + (double) (reporter->end_time.tv_nsec - reporter->start_time.tv_nsec) / 1000000000;

			sprintf(message, "%f\n", time_passed);
			
			write(STDOUT_FILENO, message, strlen(message));
		/

		}
		*/
	}
	else if(sig == SIGUSR2){
		(reporter->r_count)++;
	}
	else if(sig == SIGTERM){
		printf("Reporter count: %d\n", reporter->r_count);
		shmdt(counter);
		free(reporter);
		exit(EXIT_SUCCESS);
	}
}

void initialize_globals(){

	// Using the shm functions to create a shared memory that contains one SigusCount struct.
        shm_id = shmget(IPC_PRIVATE, sizeof(Counter), IPC_CREAT | 0666);

        counter = (Counter*) shmat(shm_id, 0, 0);

        counter->r1_received = 0;
        counter->r2_received = 0;
        counter->r1_sent = 0;
        counter->r2_sent = 0;

        // Make mutex shared across processes using the following share attribute.
        pthread_mutexattr_t shared_attr;
	pthread_mutexattr_t shared_attr2;
        pthread_mutexattr_init(&(shared_attr));
        pthread_mutexattr_setpshared(&(shared_attr), PTHREAD_PROCESS_SHARED);

	pthread_mutexattr_init(&(shared_attr2));
	pthread_mutexattr_setpshared(&(shared_attr2), PTHREAD_PROCESS_SHARED);

        //pthread_mutex_init(&(counter->r1_received_lock), &(shared_attr));
        pthread_mutex_init(&(counter->r1_lock), &(shared_attr));
	//pthread_mutex_init(&(counter->r2_received_lock), &(shared_attr2));
	pthread_mutex_init(&(counter->r2_lock), &(shared_attr2));

	pthread_mutexattr_destroy(&shared_attr);
	pthread_mutexattr_destroy(&shared_attr2);

        // Detach the shared memory.
        shmdt(counter);
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
