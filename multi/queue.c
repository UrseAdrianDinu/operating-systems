#include "queue.h"

Node *createNode(char *filename)
{
	Node *node = malloc(sizeof(Node));

	if (node == NULL)
		exit(12);

	node->filename = malloc((strlen(filename) + 1)*sizeof(char));
	if (node->filename == NULL)
		exit(12);

	strcpy(node->filename, filename);
	node->next = NULL;
	node->prev = NULL;
	return node;
}

void destroyNode(Node *node)
{
	free(node->filename);
	free(node);
}

Queue *createQueue(void)
{
	Queue *queue = malloc(sizeof(Queue));

	if (queue == NULL)
		exit(12);

	queue->size = 0;
	queue->head = NULL;
	queue->tail = NULL;
	return queue;
}

int isEmptyQueue(Queue *queue)
{
	if (queue->head == NULL || queue->tail == NULL)
		return 1;
	return 0;
}

void pushBack(Queue *queue, char *filename)
{
	Node *node = createNode(filename);

	if (node == NULL)
		return;

	if (isEmptyQueue(queue)) {
		queue->head = node;
		queue->tail = node;
	} else {
		queue->tail->next = node;
		node->prev = queue->tail;
		queue->tail = node;
	}
	queue->size++;
}

void popFront(Queue *queue)
{
	Node *node;

	if (isEmptyQueue(queue))
		return;

	node = queue->head;
	queue->head = queue->head->next;
	if (!isEmptyQueue(queue))
		queue->head->prev = NULL;
	destroyNode(node);
	queue->size--;
}

void destroyQueue(Queue *queue)
{
	while (!isEmptyQueue(queue))
		popFront(queue);
	free(queue);
}

void printQueue(Queue *queue)
{
	Node *temp = queue->head;

	while (temp) {
		printf("%s\n", temp->filename);
		temp = temp->next;
	}
}
