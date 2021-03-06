#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "busy_wait.h"

/* Lunghezza dell'iperperiodo */
#define H_PERIOD_ 30

/* Numero di frame */
#define NUM_FRAMES_ 3

/* Numero di task */
#define NUM_P_TASKS_ 5

void task0_code();
void task1_code();
void task2_code();
void task3_code();
void task4_code();

void ap_task_code();

/**********************/

/* Questo inizializza i dati globali */
const unsigned int H_PERIOD = H_PERIOD_;
const unsigned int NUM_FRAMES = NUM_FRAMES_;
const unsigned int NUM_P_TASKS = NUM_P_TASKS_;

task_routine P_TASKS[NUM_P_TASKS_];
task_routine AP_TASK;
int * SCHEDULE[NUM_FRAMES_];
int SLACK[NUM_FRAMES_];

void task_init()
{
	/* Inizializzazione di P_TASKS[] */
	P_TASKS[0] = task0_code;
	P_TASKS[1] = task1_code;
    P_TASKS[2] = task2_code;
    P_TASKS[3] = task3_code;
    P_TASKS[4] = task4_code;
	/* ... */

	/* Inizializzazione di AP_TASK */
	AP_TASK = ap_task_code;


	/* Inizializzazione di SCHEDULE e SLACK (se necessario) */

	/* frame 0 */
	SCHEDULE[0] = (int *) malloc( sizeof( int ) * 4 );
	SCHEDULE[0][0] = 0;
    SCHEDULE[0][1] = 1;
    SCHEDULE[0][2] = 2;
    SCHEDULE[0][3] = -1;

	SLACK[0] = 30; /* tutto il frame */


	/* frame 1 */
	SCHEDULE[1] = (int *) malloc( sizeof( int ) * 4 );
	SCHEDULE[1][0] = 0;
    SCHEDULE[1][1] = 1;
	SCHEDULE[1][2] = 3;
	SCHEDULE[1][3] = -1;


	SLACK[1] = 30; /* tutto il frame */


	/* frame 2 */
	SCHEDULE[2] = (int *) malloc( sizeof( int ) * 4 );
	SCHEDULE[2][0] = 0;
    SCHEDULE[2][1] = 1;
    SCHEDULE[2][2] = 4;
	SCHEDULE[2][3] = -1;

	SLACK[2] = 40; /* tutto il frame */

	/* Custom Code */
	busy_wait_init();
}

void task_destroy()
{
	unsigned int i;

	/* Custom Code */

	for ( i = 0; i < NUM_FRAMES; ++i )
		free( SCHEDULE[i] );
}

/**********************************************************/

/* Nota: nel codice dei task e' lecito chiamare ap_task_request() */

void task0_code()	//WCET = 20
{
	printf("0\n");
  busy_wait(19);
  return;
}

void task1_code()	//WCET = 20
{
	printf("1\n");
	busy_wait(19);
	return;
}

void task2_code()	//WCET = 30
{
	printf("2\n");
	busy_wait(29);
    ap_task_request();
  return;
}

void task3_code()	//WCET = 30
{
	printf("3\n");
	busy_wait(29);
  return;
}

void task4_code()	//WCET = 20
{
	printf("4\n");
	busy_wait(19);
  return;
}

void ap_task_code()	//WCET = 40
{
	printf("AP task running!\n");
	busy_wait(120);
	printf("AP task going to sleep!\n");
	return;
}
