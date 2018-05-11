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
};

void ap_task_request()
{
    return;
}

void * p_task_handler(void * a) //periodic task handler
{
    struct task * t = (struct task *) a;
    for(int i = 0; i<4*NUM_FRAMES; i++){
        pthread_mutex_lock(t->mutex);
        t->state = RUNNING;
        switch(t->id){
            case 0:
                task0_code();
                break;
            case 1:
                task1_code();
                break;
            case 2:
                task2_code();
                break;
        }
        t->state = IDLE;
        pthread_mutex_unlock(t->mutex);
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

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_t cond;
    pthread_cond_init(&cond, NULL);

    for(int i = 0; i<NUM_P_TASKS; i++){
        tasks[i].id = i;
        tasks[i].state = PENDING;
        tasks[i].mutex = &mutex;
        tasks[i].cond = &cond;
        pthread_create(&tasks[i].thread, NULL, p_task_handler, &tasks[i]);
    }

/*	while(1)
	{

		time.tv_sec += ( time.tv_nsec + nanosec ) / 1000000000;
		time.tv_nsec = ( time.tv_nsec + nanosec ) % 1000000000;
		//pthread_cond_timedwait( ..., &time );
	}*/

    for(int i = 0; i<NUM_P_TASKS;i++) pthread_join(tasks[i].thread, NULL);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

