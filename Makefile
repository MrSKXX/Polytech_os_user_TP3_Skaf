CC = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -lreadline -lpthread

all: biceps

biceps: biceps.o gescom.o creme.o
	$(CC) $(CFLAGS) -o biceps biceps.o gescom.o creme.o $(LDFLAGS)

biceps.o: biceps.c gescom.h creme.h
	$(CC) $(CFLAGS) -c biceps.c

gescom.o: gescom.c gescom.h
	$(CC) $(CFLAGS) -c gescom.c

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

memory-leak: clean
	$(CC) -g -O0 -Wall -Werror -o biceps-memory-leaks biceps.c gescom.c creme.c $(LDFLAGS)
	$(CC) -g -O0 -Wall -Werror -o biceps biceps.c gescom.c creme.c $(LDFLAGS)

clean:
	rm -f biceps biceps-memory-leaks *.o

.PHONY: all clean memory-leak