# dimensione delle matrici quardate da moltiplicare
SIZE = 10

# commentare la seguente linea affiche' non venga stampato il contenuto della matrici a video
PRINT=-DPRINT

CC = gcc
CFLAGS = -O3 -Wall $(PRINT)
LFLAGS =

all : matrix_serial matrix_threads matrix_se_timed matrix_th_timed matrix_th2_timed

matrix_serial: matrix_serial.o
	$(CC) -o matrix_serial matrix_serial.o $(LFLAGS) -lpthread

matrix_threads: matrix_threads.o
	$(CC) -o matrix_threads matrix_threads.o $(LFLAGS) -lpthread

matrix_se_timed: matrix_se_timed.o
	$(CC) -o matrix_se_timed matrix_se_timed.o $(LFLAGS)

matrix_th_timed: matrix_th_timed.o
	$(CC) -o matrix_th_timed matrix_th_timed.o $(LFLAGS) -lpthread

matrix_th2_timed: matrix_th2_timed.o
	$(CC) -o matrix_th2_timed matrix_th2_timed.o $(LFLAGS) -lpthread	

%.o: %.c 
	$(CC) $(CFLAGS) -DSIZE=$(SIZE) -c -o $@ $<

clean :
	rm -f *.o *~ matrix_serial matrix_threads matrix_se_timed matrix_th_timed matrix_th2_timed


