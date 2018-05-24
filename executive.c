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
#define ROUNDS 4	//number of hyperperiods run by the executive
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
	if(getuid() != 0 && printf("Please run this program as root.\n")) exit(1);	//check if user is root
	task_init();
	struct task exe;
	exe.mutex = mu;
	exe.cond = co;
	start_task(&exe, -2, executive, 98);	//spin up executive
	pthread_join(exe.thread, NULL);
	pthread_mutex_destroy(&exe.mutex);
	pthread_cond_destroy(&exe.cond);
	return 0;
}

void * executive(void * v) {
	struct timespec time;
	struct timeval utime;

	struct task tasks[NUM_P_TASKS];
	for(int i = 0; i<NUM_P_TASKS; i++) start_task(&tasks[i], i, p_task_handler, 99); //start periodic tasks, priority=99 to make sure they reach cond wait
	start_task(&aptask, -1, ap_task_handler, 99);	//start aperiodic task

	for(int i = 0; i<ROUNDS*NUM_FRAMES; i++) {	//main loop
		int *schedule = SCHEDULE[i%NUM_FRAMES];

		for(int a = 0; a < NUM_P_TASKS; a++) {	//check if any task has overran its frame
			if(tasks[a].state != IDLE){
				printf("Frame overrun for task %d!\n", a);
				if(tasks[a].state == PENDING){	//if task is pending, abort it
					tasks[a].state = IDLE;
					printf("Aborting task %d...\n", a);
				}
			}
		}

		if(run_ap && aptask.state != IDLE) {	//check if ap task has missed its deadline
			printf("ERROR! AP task was released before its previous instance finished!\n");
		} else if(run_ap) {	//if it hasn't, release it for a new cycle
			set_priority(&aptask, 1);
			lock_signal(&aptask);
		}

		for(int a = 0; a < sizeof(schedule); a++) {	//period task release cycle
			if(schedule[a] == -1) break;	//frame end
			if(tasks[schedule[a]].state == IDLE) {	//only release a task if its state is IDLE
				set_priority(&tasks[schedule[a]], 96-a);	//priority decreases in order
				lock_signal(&tasks[schedule[a]]);
			} else printf("Skipping release this cycle for task %d...\n", schedule[a]);
		}

		gettimeofday(&utime,NULL);	//get time reference for frame timings
		if(aptask.state != IDLE) {
			set_priority(&aptask, 97);	//set ap task priority higher than periodic tasks to allow slack stealing
			run_ap = 0;
			exec_wait(&time, &utime, (SLACK[i%NUM_FRAMES])*1000000);	//wait slack time or until ap task is finished
			set_priority(&aptask, 97 - NUM_P_TASKS);	//lower ap task priority so it doesn't interfere with periodic tasks
		}

		exec_wait(&time, &utime, H_PERIOD*10000000/NUM_FRAMES);	//wait until the end of the frame
		printf("#########\n");
	}
	for(int i = 0; i < NUM_P_TASKS; i++) end_task(&tasks[i]);	//end periodic tasks
	end_task(&aptask);	//end aperiodic task
	printf("returning...\n");
	return NULL;
}

void ap_task_request() {
	run_ap = 1;
}

void * p_task_handler(void * a) {	//periodic task handler
	struct task * t = (struct task *) a;
	pthread_mutex_lock(&t->mutex);
	while(1) {
		pthread_cond_wait(&t->cond, &t->mutex);
		if(t->state != IDLE) {	//if state is IDLE at this point it means the task was aborted
			t->state = RUNNING;
			pthread_mutex_unlock(&t->mutex);
			P_TASKS[t->id]();	//task's code
			pthread_mutex_lock(&t->mutex);
			t->state = IDLE;
		}
	}
}

void * ap_task_handler(void * a) {	//aperiodic task handler
	struct task * t = (struct task *) a;
	pthread_mutex_lock(&t->mutex);
	while(1) {
		pthread_cond_wait(&t->cond, &t->mutex);
		t->state = RUNNING;
		pthread_mutex_unlock(&t->mutex);
		AP_TASK();	//ap task code
		pthread_mutex_lock(&t->mutex);
		t->state = IDLE;
		pthread_mutex_lock(&mu);
		pthread_cond_signal(&co);	//if the AP task's execution is shorter than the slack time, signal the executive to stop waiting
		pthread_mutex_unlock(&mu);
	}
}

void init_attr(int priority, pthread_attr_t *attr) {	//initialize thread attributes with given priority
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

void start_task(struct task * t, int id, void *(* start_routine) (void *), int priority) {	//create thread for given task with given priority
	pthread_attr_t attr;
	init_attr(priority, &attr);	//initialize thread attributes
	t->id = id;
	t->state = IDLE;
	pthread_mutex_init(&t->mutex, NULL);
	pthread_cond_init(&t->cond, NULL);
	pthread_create(&t->thread, &attr, start_routine, t);
}

void set_priority(struct task * t, int priority) {	//dynamically set task priority
	struct sched_param param;
	param.sched_priority = priority;
	pthread_mutex_lock(&t->mutex);
	pthread_setschedparam(t->thread, SCHED_FIFO, &param);
	pthread_mutex_unlock(&t->mutex);
}

void lock_signal(struct task * t) {	//lock task's mutex, signal task's cond, unlock task's mutex
	pthread_mutex_lock(&t->mutex);
	t->state = PENDING;	//also set task's status to pending
	pthread_cond_signal(&t->cond);
	pthread_mutex_unlock(&t->mutex);
}

void end_task(struct task * t) {	//kill and join task's thread
		pthread_cancel(t->thread);
		pthread_join(t->thread, NULL);
		pthread_mutex_destroy(&t->mutex);
		pthread_cond_destroy(&t->cond);
}

void exec_wait(struct timespec * time, struct timeval * utime, int nanosec) {	//put the executive to sleep until its cond is signaled or the given time has elapsed
	time->tv_sec = utime->tv_sec;
	time->tv_nsec = utime->tv_usec * 1000;
	time->tv_sec += ( time->tv_nsec + nanosec ) / 1000000000;
	time->tv_nsec = ( time->tv_nsec + nanosec ) % 1000000000;
	pthread_mutex_lock(&mu);
	pthread_cond_timedwait(&co, &mu, time);
	pthread_mutex_unlock(&mu);
}
