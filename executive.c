/* traccia dell'executive (pseudocodice) */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "task.h"

struct task{
    pthread_t thread;
    unsigned int id;
};

void ap_task_request()
{
    return;
}

void p_task_handler() //periodic task handler
{
	return;
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
        pthread_create(&tasks[i].thread, NULL, p_task_handler, &tasks[i]);
    }

/*	while(1)
	{

		time.tv_sec += ( time.tv_nsec + nanosec ) / 1000000000;
		time.tv_nsec = ( time.tv_nsec + nanosec ) % 1000000000;
		//pthread_cond_timedwait( ..., &time );
	}*/

    for(int i = 0; i<NUM_P_TASKS;i++) pthread_join(tasks[i].thread, NULL);
}

