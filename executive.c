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
};

int quit = 0;
int run_ap = 0;
struct task aptask;

void ap_task_request()
{
	if(aptask.state == IDLE) run_ap = 1;
	else printf("ERROR! AP task was released before its previous instance finished!\n");
}

void * p_task_handler(void * a) //periodic task handler
{
	struct task * t = (struct task *) a;
	pthread_mutex_lock(&t->mutex);
	while(1){
		pthread_cond_wait(&t->cond, &t->mutex);
		if(quit == 1){
			printf("quitting... %d\n", t->id);
			return NULL;
		}
		t->state = RUNNING;
		pthread_mutex_unlock(&t->mutex);
		P_TASKS[t->id]();
		pthread_mutex_lock(&t->mutex);
		if(t->id == 2) ap_task_request();
		t->state = IDLE;
	}
}

void * ap_task_handler(void * a){
	struct task * t = (struct task *) a;
	pthread_mutex_lock(&t->mutex);
	while(1){
		pthread_cond_wait(&t->cond, &t->mutex);
		if(quit == 1){
			printf("quitting... %d\n", t->id);
			return NULL;
		}
		t->state = RUNNING;
		pthread_mutex_unlock(&t->mutex);
		AP_TASK();
		pthread_mutex_lock(&t->mutex);
		t->state = IDLE;
	}
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
		param.sched_priority = 99;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
		pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
		pthread_attr_setschedparam(&attr, &param);
#ifdef MULTIPROC
  	CPU_ZERO(&cpuset);
  	CPU_SET(0, &cpuset);
  	pthread_attr_setaffinity_np( &attr, sizeof(cpu_set_t), &cpuset );
#endif
		tasks[i].id = i;
		tasks[i].state = IDLE;
		pthread_mutex_init(&tasks[i].mutex, NULL);
		pthread_cond_init(&tasks[i].cond, NULL);
		pthread_create(&tasks[i].thread, &attr, p_task_handler, &tasks[i]);
	}

	//AP TASK
	struct sched_param apparam;
	apparam.sched_priority = 1;
	pthread_attr_t apattr;
	pthread_attr_init(&apattr);
	pthread_attr_setinheritsched( &apattr, PTHREAD_EXPLICIT_SCHED );
	pthread_attr_setschedpolicy(&apattr, SCHED_FIFO);
	pthread_attr_setschedparam(&apattr, &apparam);
#ifdef MULTIPROC
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np( &apattr, sizeof(cpu_set_t), &cpuset );
#endif

	aptask.id = -1;
	aptask.state = IDLE;
	pthread_mutex_init(&aptask.mutex, NULL);
	pthread_cond_init(&aptask.cond, NULL);
	pthread_create(&aptask.thread, &apattr, ap_task_handler, &aptask);

	///////

	int skip = 0;

	for(int i = 0; i<ROUNDS*NUM_FRAMES; i++){	//main loop
		int *schedule = SCHEDULE[i%NUM_FRAMES];
		for(int a = 0; a < NUM_P_TASKS; a++) if(tasks[a].state != IDLE) skip = 1; //if a task is running or pending skip this frame

		int nanosec = 0;

		if(run_ap && !skip){
			struct sched_param param;
			param.sched_priority = 98-NUM_P_TASKS;
			pthread_mutex_lock(&aptask.mutex);
			aptask.state = PENDING;
			pthread_setschedparam(aptask.thread, SCHED_FIFO, &param);
			pthread_cond_signal(&aptask.cond);
			pthread_mutex_unlock(&aptask.mutex);
			run_ap = 0;
		}

		if(aptask.state != IDLE){
			nanosec = (SLACK[i%NUM_FRAMES])*1000000;
			// printf("waiting %d ns\n", nanosec);
			gettimeofday(&utime,NULL);

			time.tv_sec = utime.tv_sec;
			time.tv_nsec = utime.tv_usec * 1000;
			time.tv_sec += ( time.tv_nsec + nanosec ) / 1000000000;
			time.tv_nsec = ( time.tv_nsec + nanosec ) % 1000000000;
			pthread_mutex_lock(&mu);
			pthread_cond_timedwait(&co, &mu, &time );
			pthread_mutex_unlock(&mu);
		}

		for(int a = 0; a < sizeof(schedule); a++){
			if(skip != 1){
				if(schedule[a] == -1) break;
				struct sched_param param;
				param.sched_priority = 97 - a;
				pthread_mutex_lock(&tasks[schedule[a]].mutex);
				tasks[schedule[a]].state = PENDING;
				pthread_setschedparam(tasks[schedule[a]].thread, SCHED_FIFO, &param);
				pthread_cond_signal(&tasks[schedule[a]].cond);
				pthread_mutex_unlock(&tasks[schedule[a]].mutex);
			}
		}

		if(skip){
			printf("ERROR! Frame overrun! Skipping this frame!\n");
			skip = 0;
		}
		nanosec = H_PERIOD*10000000/NUM_FRAMES - nanosec;
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
	for(int a = 0; a < NUM_P_TASKS; a++) pthread_cancel(tasks[a].thread);

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
	struct sched_param param;
	param.sched_priority = 98;
	pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
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
