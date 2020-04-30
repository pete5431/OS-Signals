CC = gcc

CFLAGS = -c -Wall -I

all:
	gcc -o prog1 prog1.c -pthread
	gcc -o prog2 prog2.c -pthread

clean:
	-rm *.o prog1
	-rm *.o prog2
