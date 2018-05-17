/* traccia dell'executive (pseudocodice) */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include "task.h"

#define PENDING 0
#define RUNNING 1
#define IDLE 2

struct task{
	pthread_t thread;
	unsigned int id;
	pthread_mutex_t * mutex;
	pthread_cond_t * cond;
	int state;
	int priority;
};

void ap_task_request()
{
	return;
}

void * p_task_handler(void * a) //periodic task handler
{
	struct task * t = (struct task *) a;
	for(int i = 0; i<4*NUM_FRAMES; i++){
		pthread_cond_wait(t->cond, t->mutex);
		pthread_mutex_lock(t->mutex);
		t->state = RUNNING;
		P_TASKS[t->id]();
		t->state = IDLE;
	}
	return NULL;
}

void ap_task_handler()
{
	return;
}

void executive()
{
	struct timespec time;
	struct timeval utime;

	gettimeofday(&utime,NULL);

	time.tv_sec = utime.tv_sec;
	time.tv_nsec = utime.tv_usec * 1000;

	struct task tasks[NUM_P_TASKS];

	for(int i = 0; i<NUM_P_TASKS; i++){
		tasks[i].id = i;
		tasks[i].state = PENDING;
		pthread_mutex_init(tasks[i].mutex, NULL);
		pthread_cond_init(tasks[i].cond, NULL);
		pthread_create(&tasks[i].thread, NULL, p_task_handler, &tasks[i]);
		tasks[i].priority = 98 - i; //99 is the executive
	}

	for(int i = 0; i<4*NUM_FRAMES; i++){	//main loop
		int * schedule = SCHEDULE[i%NUM_FRAMES];
		for(int a = 0; a< sizeof(schedule); a++){
			if(schedule[a] == -1) break;
			pthread_cond_signal(tasks[schedule[a]].cond);
		}
		// time.tv_sec += ( time.tv_nsec + nanosec ) / 1000000000;
		// time.tv_nsec = ( time.tv_nsec + nanosec ) % 1000000000;
		//pthread_cond_timedwait( ..., &time );
	}

	for(int i = 0; i<NUM_P_TASKS;i++) pthread_join(tasks[i].thread, NULL);
	for(int i = 0; i<NUM_P_TASKS; i++){
		pthread_mutex_destroy(tasks[i].mutex);
		pthread_cond_destroy(tasks[i].cond);
	}
}

int main(){
	executive();
	return 0;
}
