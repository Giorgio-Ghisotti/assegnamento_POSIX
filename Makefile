CC = gcc
CFLAGS = -O3 -Wall $(PRINT)
LFLAGS = -DMULTIPROC -g

all : executive

executive: busy_wait.o executive.o task.o
	$(CC) -o executive busy_wait.o executive.o task.o $(LFLAGS) -lpthread

%.o: %.c 
	$(CC) $(CFLAGS) -DSIZE=$(SIZE) -c -o $@ $< $(LFLAGS)

clean :
	rm -f *.o *~ executive


