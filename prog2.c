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
#include <fcntl.h>

#define BILLION 1000000000;

typedef struct{

	int r1_received;
	int r2_received;
	int r1_sent;
	int r2_sent;

	pthread_mutex_t r1_received_ lock;
	pthread_mutex_t r1_sent_lock;
	pthread_mutex_t r2_received_lock;
	pthread_mutex_t_t2_sent_lock;

} Counter;

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

Counter* counter;
Reporter* reporter;

int shm_id_counter;
int shm_id_reporter;

pthread_t threads[8];

void create_threads();
void* r1_handler_thread(void* arg);
void* r2_handler_thread(void* arg);
void* signaler_thread(void* arg);
void* reporter_thread(void* arg);

void initialize_counter();
void intialize_reporter();

void block_SIGUSR1();
void unblock_SIGUSR1();
void block_SIGUSR2();
void unblock_SIGUSR2();
void block_SIGTERM();
void unblock_SIGTERM();

// Parent thread.
int main(int argc, char* argv[]){

	srand(time(NULL));

	initialize_counter();
	initialize_reporter();

	block_SIGUSR1();
	block_SIGUSR2();

	counter = (Counter*) shmat(shm_id_counter, 0, 0);

	create_threads();

	if(argc == 2){
		int total = atoi(argv[1]);
	
	}		

	return 0;
}

void* r1_handler_thread(void* arg){


	

}

void* r2_handler_thread(void* arg){

}

void* signaler_thread(void* arg){


}

void* reporter_thread(void* arg){


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

void create_threads(){

	reporter = (Reporter*) shmat(shm_id_reporter, 0, 0);

	if(pthread_create(&threads[0], NULL, reporter_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }
	shmdt(reporter);
	               
        if(pthread_create(&threads[1], NULL, r1_handler_thread, NULL) != 0){
                printf("Thread create failed.\n");
                exit(1);
        }               
        if(pthread_craete(&threads[2], NULL, r1_handler_thread, NULL) != 0){
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

