CC = gcc
CFLAGS = -Wall -g
 
build: libso_stdio.so


libso_stdio.so: so_stdio.o
	$(CC) -shared so_stdio.o -o libso_stdio.so
 
so_stdio.o: so_stdio.c
	$(CC) $(CFLAGS) -c so_stdio.c
 
.PHONY: clean
 
clean:
	rm -f *.o libso_stdio.so
