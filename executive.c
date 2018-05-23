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

struct task {
	pthread_t thread;
	unsigned int id;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int state;
};

int run_ap = 0;	//is there an aperiodic task waiting to be scheduled?
struct task aptask;	//aperiodic task

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;	//EXECUTIVE's semaphores
pthread_cond_t co = PTHREAD_COND_INITIALIZER;

void ap_task_request();
void * p_task_handler(void * a);
void * ap_task_handler(void * a);
void init_attr(int priority, pthread_attr_t *attr);
void start_task(struct task * t, int id, void *(* start_routine) (void *), int priority);
void set_priority(struct task * t, int priority);
void lock_signal(struct task * t);
void end_task(struct task * t);
void exec_wait(struct timespec * time, struct timeval * utime, int nanosec);
void * executive(void * v);

int main() {
	if(getuid() != 0 && printf("Please run this program as root.\n")) exit(1);
	task_init();
	struct task exe;
	start_task(&exe, -2, executive, 98);
	pthread_join(exe.thread, NULL);
	return 0;
}

void * executive(void * v) {
	struct timespec time;
	struct timeval utime;

	struct task tasks[NUM_P_TASKS];
	for(int i = 0; i<NUM_P_TASKS; i++) start_task(&tasks[i], i, p_task_handler, 99);
	start_task(&aptask, -1, ap_task_handler, 99);

	int skip = 0;

	for(int i = 0; i<ROUNDS*NUM_FRAMES; i++) {	//main loop
		int *schedule = SCHEDULE[i%NUM_FRAMES];
		for(int a = 0; a < NUM_P_TASKS; a++) if(tasks[a].state != IDLE) skip = 1; //if a task is running or pending skip this frame
		int nanosec = 0;

		if(run_ap && !skip) {
			set_priority(&aptask, 1);
			lock_signal(&aptask);
		}

		for(int a = 0; a < sizeof(schedule); a++) {
			if(skip != 1) {
				if(schedule[a] == -1) break;
				set_priority(&tasks[schedule[a]], 96-a);
				lock_signal(&tasks[schedule[a]]);
			}
		}

		gettimeofday(&utime,NULL);
		if(aptask.state != IDLE) {
			nanosec = (SLACK[i%NUM_FRAMES])*1000000;
			set_priority(&aptask, 97);
			run_ap = 0;
			exec_wait(&time, &utime, nanosec);
			set_priority(&aptask, 97 - NUM_P_TASKS);
		}

		if(skip) {
			printf("ERROR! Frame overrun! Skipping this frame!\n");
			skip = 0;
		}
		nanosec = H_PERIOD*10000000/NUM_FRAMES;
		exec_wait(&time, &utime, nanosec);
		printf("#########\n");
	}
	for(int i = 0; i < NUM_P_TASKS; i++) end_task(&tasks[i]);
	end_task(&aptask);
	printf("returning...\n");
	return NULL;
}

void ap_task_request() {
	if(aptask.state == IDLE) run_ap = 1;
	else printf("ERROR! AP task was released before its previous instance finished!\n");
}

void * p_task_handler(void * a) {	//periodic task handler
	struct task * t = (struct task *) a;
	pthread_mutex_lock(&t->mutex);
	while(1) {
		pthread_cond_wait(&t->cond, &t->mutex);
		t->state = RUNNING;
		pthread_mutex_unlock(&t->mutex);
		P_TASKS[t->id]();	//task's code
		pthread_mutex_lock(&t->mutex);
		if(t->id == 2) ap_task_request();
		t->state = IDLE;
	}
}

void * ap_task_handler(void * a) {	//aperiodic task handler
	struct task * t = (struct task *) a;
	pthread_mutex_lock(&t->mutex);
	while(1) {
		pthread_cond_wait(&t->cond, &t->mutex);
		t->state = RUNNING;
		pthread_mutex_unlock(&t->mutex);
		AP_TASK();
		pthread_mutex_lock(&t->mutex);
		t->state = IDLE;
		pthread_mutex_lock(&mu);
		pthread_cond_signal(&co);	//if the AP task's execution is shorter than the slack time, signal the executive to stop waiting
		pthread_mutex_unlock(&mu);
	}
}

void init_attr(int priority, pthread_attr_t *attr) {	//initialize thread attributes with given parameters and priority
	struct sched_param param;
	param.sched_priority = priority;
	pthread_attr_init(attr);
	pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(attr, SCHED_FIFO);
	pthread_attr_setschedparam(attr, &param);
#ifdef MULTIPROC
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), &cpuset);
#endif
}

void start_task(struct task * t, int id, void *(* start_routine) (void *), int priority) {	//create thread for given task
	pthread_attr_t attr;
	init_attr(priority, &attr);	//initialize thread attributes
	t->id = id;
	t->state = IDLE;
	pthread_mutex_init(&t->mutex, NULL);
	pthread_cond_init(&t->cond, NULL);
	pthread_create(&t->thread, &attr, start_routine, t);
}

void set_priority(struct task * t, int priority) {
	struct sched_param param;
	param.sched_priority = priority;
	pthread_mutex_lock(&t->mutex);
	pthread_setschedparam(t->thread, SCHED_FIFO, &param);
	pthread_mutex_unlock(&t->mutex);
}

void lock_signal(struct task * t) {
	pthread_mutex_lock(&t->mutex);
	t->state = PENDING;
	pthread_cond_signal(&t->cond);
	pthread_mutex_unlock(&t->mutex);
}

void end_task(struct task * t) {
		pthread_cancel(t->thread);
		pthread_join(t->thread, NULL);
		pthread_mutex_destroy(&t->mutex);
		pthread_cond_destroy(&t->cond);
}

void exec_wait(struct timespec * time, struct timeval * utime, int nanosec) {
	time->tv_sec = utime->tv_sec;
	time->tv_nsec = utime->tv_usec * 1000;
	time->tv_sec += ( time->tv_nsec + nanosec ) / 1000000000;
	time->tv_nsec = ( time->tv_nsec + nanosec ) % 1000000000;
	pthread_mutex_lock(&mu);
	pthread_cond_timedwait(&co, &mu, time);
	pthread_mutex_unlock(&mu);
}
