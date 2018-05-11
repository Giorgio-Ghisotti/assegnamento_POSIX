CC = gcc
CFLAGS = -O3 -Wall $(PRINT)
LFLAGS = 

all : busy_wait

busy_wait: busy_wait.o executive.o task.o
	$(CC) -o busy_wait busy_wait.o executive.o task.o $(LFLAGS) -lpthread

%.o: %.c 
	$(CC) $(CFLAGS) -DSIZE=$(SIZE) -c -o $@ $<

clean :
	rm -f *.o *~ busy_wait


