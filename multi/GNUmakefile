CC = gcc
CFLAGS = -Wall -g

build: so-cpp

so-cpp: so-cpp.o queue.o hashmap.o
	$(CC) $(CFLAGS) so-cpp.o queue.o hashmap.o -o so-cpp

so-cpp.o: so-cpp.c
	$(CC) $(CFLAGS) -c so-cpp.c

queue.o:queue.c
	$(CC) $(CFLAGS) -c queue.c

hashmap.o: hashmap.c
	$(CC) $(CFLAFS) -c hashmap.c

clean:
	rm *.o so-cpp