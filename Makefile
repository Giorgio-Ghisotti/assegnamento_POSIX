CC = gcc
CFLAGS = -O3 -Wall $(PRINT)
LFLAGS =

all : executive

busy_wait: busy_wait.o
	$(CC) -o busy_wait busy_wait.o $(LFLAGS) -lpthread

executive: executive.o busy_wait.o task.o
	$(CC) -o executive executive.o busy_wait.o task.o $(LFLAGS) -lpthread

%.o: %.c 
	$(CC) $(CFLAGS) -DSIZE=$(SIZE) -c -o $@ $<

clean :
	rm -f *.o *~ executive busy_wait


