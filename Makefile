CC = gcc
CFLAGS = -O3 -Wall $(PRINT)
LFLAGS = -DMULTIPROC

all : executive

executive: busy_wait.o executive.o task.o
	$(CC) -o executive busy_wait.o executive.o task.o $(LFLAGS) -lpthread

%.o: %.c 
	$(CC) $(CFLAGS) -DSIZE=$(SIZE) -c -o $@ $<

clean :
	rm -f *.o *~ executive


