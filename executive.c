/* traccia dell'executive (pseudocodice) */
#ifdef MULTIPROC
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <sched.h>
#include "task.h"
#include "busy_wait.h"

#define PENDING 0
#define RUNNING 1
#define IDLE 2
#define ROUNDS 4
#ifdef MULTIPROC
	cpu_set_t cpuset;
#endif


struct task{
	pthread_t thread;
	unsigned int id;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int state;
	int cycles;
};

void ap_task_request()
{
	return;
}

void * p_task_handler(void * a) //periodic task handler
{
	struct task * t = (struct task *) a;
	for(int i = 0; i<t->cycles*ROUNDS; i++){
		pthread_cond_wait(&t->cond, &t->mutex);
		t->state = RUNNING;
		P_TASKS[t->id]();
		t->state = IDLE;
	}
	printf("returning... %d\n", t->id);
	return NULL;
}

void ap_task_handler()
{
	return;
}

void * executive(void * v)
{
	struct timespec time;
	struct timeval utime;

	pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t co = PTHREAD_COND_INITIALIZER;

	struct task tasks[NUM_P_TASKS];

	for(int i = 0; i<NUM_P_TASKS; i++){
		struct sched_param param;
		param.sched_priority = 98 - i;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
		pthread_attr_setschedparam(&attr, &param);
#ifdef MULTIPROC
  	CPU_ZERO(&cpuset);
  	CPU_SET(0, &cpuset);
  	pthread_attr_setaffinity_np( &attr, sizeof(cpu_set_t), &cpuset );
#endif
		tasks[i].id = i;
		tasks[i].state = IDLE;
		tasks[i].cycles = 0;
		for(int b = 0; b<NUM_FRAMES; b++){
			int c = 0;
			while(SCHEDULE[b][c] != -1){
				if(SCHEDULE[b][c] == i) tasks[i].cycles++;
				c++;
			}
		}
		pthread_mutex_init(&tasks[i].mutex, NULL);
		pthread_cond_init(&tasks[i].cond, NULL);
		pthread_create(&tasks[i].thread, &attr, p_task_handler, &tasks[i]);
	}

	sleep(4);

	int skip = 0;

	for(int i = 0; i<ROUNDS*NUM_FRAMES; i++){	//main loop
		int *schedule = SCHEDULE[i%NUM_FRAMES];
		for(int a = 0; a < NUM_P_TASKS; a++) if(tasks[a].state != IDLE) skip = 1; //if a task is running or pending skip this frame
		for(int a = 0; a < sizeof(schedule); a++){
			if(!skip){
				if(schedule[a] == -1) break;
				struct sched_param param;
				param.sched_priority = 98 - a;
				pthread_mutex_lock(&tasks[schedule[a]].mutex);
				tasks[schedule[a]].state = PENDING;
				pthread_setschedparam(tasks[schedule[a]].thread, SCHED_FIFO, &param);
				pthread_cond_signal(&tasks[schedule[a]].cond);
				pthread_mutex_unlock(&tasks[schedule[a]].mutex);
			}
		}

		if(skip){
			printf("Frame overrun! Skipping this frame!\n");
			skip = 0;
		}
		//int nanosec = H_PERIOD*100000/NUM_FRAMES;
		int nanosec = 1000000000;
		// printf("waiting... utime.tv_usec: %ld\n", utime.tv_usec);
		gettimeofday(&utime,NULL);

		time.tv_sec = utime.tv_sec;
		time.tv_nsec = utime.tv_usec * 1000;
		time.tv_sec += ( time.tv_nsec + nanosec ) / 1000000000;
		time.tv_nsec = ( time.tv_nsec + nanosec ) % 1000000000;
		pthread_mutex_lock(&mu);
		pthread_cond_timedwait(&co, &mu, &time );
		pthread_mutex_unlock(&mu);
		printf("#########\n");
		// printf("waited %ld ns; utime.tv_sec: %ld\n",time.tv_nsec - utime.tv_usec*1000, utime.tv_sec);
	}

	for(int i = 0; i<NUM_P_TASKS;i++) pthread_join(tasks[i].thread, NULL);
	for(int i = 0; i<NUM_P_TASKS; i++){
		pthread_mutex_destroy(&tasks[i].mutex);
		pthread_cond_destroy(&tasks[i].cond);
	}
	printf("returning...\n");
	return NULL;
}

int main(){

	task_init();
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	struct sched_param param;
	param.sched_priority = 99;
	pthread_attr_setschedparam(&attr, &param);

#ifdef MULTIPROC
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np( &attr, sizeof(cpu_set_t), &cpuset );
#endif

	pthread_t exe;
	pthread_create(&exe, &attr, executive, NULL);
	pthread_join(exe, NULL);
	return 0;
}
