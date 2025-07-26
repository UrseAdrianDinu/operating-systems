#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct Node {
	char *filename;
	struct Node *next;
	struct Node *prev;
} Node;

typedef struct Queue {
	long size;
	Node *head;
	Node *tail;
} Queue;

Node *createNode(char *filename);

void destroyNode(Node *node);

Queue *createQueue(void);

void destroyQueue(Queue *queue);

int isEmptyQueue(Queue *queue);

void pushBack(Queue *queue, char *filename);

void popFront(Queue *queque);

void printQueue(Queue *queue);

