CC = gcc
CFLAGS = -fPIC -m32 -Wall -g
LDFLAGS = -m32

.PHONY: build
build: libso_loader.so

libso_loader.so: loader.o exec_parser.o
	$(CC) $(LDFLAGS) -shared -o $@ $^ -lm

exec_parser.o: exec_parser.c exec_parser.h
	$(CC) $(CFLAGS) -o $@ -c $< -lm

loader.o: loader.c
	$(CC) $(CFLAGS) -o $@ -c $< -lm

.PHONY: clean
clean:
	-rm -f exec_parser.o loader.o libso_loader.so
