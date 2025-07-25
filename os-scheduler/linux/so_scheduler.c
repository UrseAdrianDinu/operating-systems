#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include "so_scheduler.h"
#include "utils.h"

// Starile unui thread
enum state {
	NEW,
	READY,
	RUNNING,
	WAITING,
	TERMINATED
};

// Structura ce reprezinta un thread
typedef struct my_thread {
	unsigned int prio;
	unsigned int time;
	int io;
	pthread_t thread;
	enum state state;
	so_handler *func;
	sem_t ready;
	sem_t run;

} my_thread;

// Structura ce reprezinta un nod din lista
typedef struct thread_node {
	my_thread *thread_data;
	struct thread_node *next;
} thread_node;

// Structura ce reprezinta scheduler-ul
typedef struct my_scheduler {
	unsigned int time_quantum;
	unsigned int io;
	int ready;
	my_thread *running;
	thread_node **queue;
	thread_node *tlist;

	sem_t finish;

} my_scheduler;

static my_scheduler *scheduler;

// Functie care insereaza un thread
// in coada de prioritati asociata
void enqueue_prio_queue(my_thread *thread)
{
	thread_node *node = malloc(sizeof(thread_node));

	DIE(node == NULL, "malloc");

	thread_node *curr = NULL;

	node->thread_data = thread;
	node->next = NULL;
	if (scheduler->queue[thread->prio] == NULL)
		scheduler->queue[thread->prio] = node;
	else {
		curr = scheduler->queue[thread->prio];
		while (curr->next != NULL)
			curr = curr->next;
		curr->next = node;
	}
}

// Functie care insereaza un thread
// in lista de thread-uri
void enqueue_threads_list(my_thread *thread)
{
	thread_node *node = malloc(sizeof(thread_node));

	DIE(node == NULL, "malloc");
	node->thread_data = thread;
	if (scheduler->tlist == NULL) {
		node->next = NULL;
		scheduler->tlist = node;
	} else {
		node->next = scheduler->tlist;
		scheduler->tlist = node;
	}
}

// Functie care scoate un thread din coada
void dequeue(thread_node **queue, my_thread *thread)
{
	thread_node *head = *queue;
	thread_node *aux = NULL;

	if (head != NULL) {
		if (head->thread_data->thread == thread->thread) {
			*queue = (*queue)->next;
			free(head);
		} else {
			if (head->next != NULL) {
				while (head->next->thread_data->thread != thread->thread)
					head = head->next;
				aux = head->next;
				head->next = head->next->next;
				free(aux);
			} else {
				aux = head;
				head = head->next;
				free(aux);
				*queue = NULL;
			}
		}
	}
}

// Functie care intoarce thread-ul care are
// cea mai mare prioritate
my_thread *get_highest_prio_thread(void)
{
	my_thread *running = scheduler->running;
	thread_node *temp = NULL;

	for (int i = 5; i >= 0; i--) {
		temp = scheduler->queue[i];
		while (temp) {
			if (temp->thread_data->state != WAITING &&
					temp->thread_data->thread != running->thread)
				return temp->thread_data;
			temp = temp->next;
		}
	}
	return NULL;
}

/*
 * creates and initializes scheduler
 * + time quantum for each thread
 * + number of IO devices supported
 * returns: 0 on success or negative on error
 */

DECL_PREFIX int so_init(unsigned int time_quantum, unsigned int io)
{
	int ret;

    // Verificarea parametrilor
	if (time_quantum <= 0 || io > 256)
		return -1;

    // Verificare daca scheduler-ul
    // a mai fost initializat
	if (scheduler != NULL)
		if (scheduler->ready == 1)
			return -1;

    // Initializare scheduler
	scheduler = malloc(sizeof(my_scheduler));
	DIE(scheduler == NULL, "malloc");
	if (scheduler == NULL)
		return -1;
	scheduler->ready = 1;
	scheduler->io = io;
	scheduler->time_quantum = time_quantum;
	scheduler->queue = malloc(6 * sizeof(struct thread_node));
	DIE(scheduler->queue == NULL, "malloc");
	for (int i = 0; i <= 5; i++)
		scheduler->queue[i] = NULL;
	scheduler->running = NULL;
	scheduler->tlist = NULL;
	ret = sem_init(&scheduler->finish, 0, 0);
	DIE(ret == -1, "sem_init");
	return 0;
}

// Functie care deblocheaza thread-ul care
// are cea mai mare prioritate daca exista
// in caz contrar deblocheaza semaforul finish
void run_next_thread(void)
{
	my_thread *t = get_highest_prio_thread();
	int ret;

	if (t != NULL) {
		scheduler->running = t;
		ret = sem_post(&t->run);
		DIE(ret == -1, "sem_post");
	} else {
		ret = sem_post(&scheduler->finish);
		DIE(ret == -1, "sem_post");
	}
}

// Functia executata de fiecare thread
void *start_thread(void *args)
{
	my_thread *thread = (struct my_thread *)args;
	int ret;

    // Inserare thread in lista de thead-uri
    // si in coada de prioritati
	enqueue_prio_queue(thread);
	enqueue_threads_list(thread);
    // Anunta scheduler-ul ca thread-ul este ready
	ret = sem_post(&thread->ready);
	DIE(ret == -1, "sem_wait");
	thread->state = READY;
    // Asteapta sa fie deblocat
	ret = sem_wait(&thread->run);
	DIE(ret == -1, "sem_wait");
	thread->state = RUNNING;
    // Ruleaza handler-ul
	thread->func(thread->prio);
    // Thread-ul este scos din coada
	dequeue(&scheduler->queue[thread->prio], thread);
	thread->state = TERMINATED;

    // Ruleaza urmatorul thread
	run_next_thread();
	return NULL;
}

/*
 * creates a new so_task_t and runs it according to the scheduler
 * + handler function
 * + priority
 * returns: tid of the new task if successful or INVALID_TID
 */

DECL_PREFIX tid_t so_fork(so_handler *func, unsigned int priority)
{
	int ret;
	// Verificarea parametrilor
	if (priority > SO_MAX_PRIO || func == NULL)
		return INVALID_TID;

	// Initializare structura thread
	my_thread *mthread = malloc(sizeof(my_thread));

	DIE(mthread == NULL, "malloc");
	mthread->prio = priority;
	mthread->func = func;
	mthread->state = NEW;
	mthread->time = scheduler->time_quantum;
	mthread->io = -1;
	ret = sem_init(&mthread->ready, 0, 0);
	DIE(ret == -1, "sem_init");
	ret = sem_init(&mthread->run, 0, 0);
	DIE(ret == -1, "sem_init");
	// Creare thread
	if (pthread_create(&mthread->thread, NULL, start_thread, mthread) != 0)
		return INVALID_TID;

	// Asteapta thread-ul sa intre in starea READY
	ret = sem_wait(&mthread->ready);
	DIE(ret == -1, "sem_wait");

	// In cazul in care nu exista niciun thread
	// care ruleaza, deblochez thread-ul curent
	// Altfel, apelez so_exec pentru a consuma timp
	// pe procesor si pentru a verifica daca trebuie preemptat
	if (scheduler->running == NULL) {
		scheduler->running = mthread;
		ret = sem_post(&mthread->run);
		DIE(ret == -1, "sem_post");
	} else
		so_exec();
	return mthread->thread;
}

/*
 * waits for an IO device
 * + device index
 * returns: -1 if the device does not exist or 0 on success
 */

DECL_PREFIX int so_wait(unsigned int io)
{
    // Verificare parametri
	if (io < 0 || io >= scheduler->io)
		return -1;

	my_thread *running = scheduler->running;
	my_thread *t = NULL;
	int ret;

    // Setez starea thread-ului care ruleaza
    // la WAITING, ii resetez timpul
    // si ii setez operatia io la care asteapta
	running->state = WAITING;
	running->time = scheduler->time_quantum;
	running->io = io;
    // Pornesc urmatorul thread
	t = get_highest_prio_thread();
	if (t != NULL) {
		scheduler->running = t;
		ret = sem_post(&t->run);
		DIE(ret == -1, "sem_post");
	}
    // Thread-ul running asteapta
    // pana cand este deblocat
	ret = sem_wait(&running->run);
	DIE(ret == -1, "sem_wait");
	return 0;
}

/*
 * signals an IO device
 * + device index
 * return the number of tasks woke or -1 on error
 */
DECL_PREFIX int so_signal(unsigned int io)
{
    // Verificarea parametrilor
	if (io < 0 || io >= scheduler->io)
		return -1;

	int k = 0;
	thread_node *temp = NULL;

    // Parcurg cozile de prioritati
    // Numar cate thread-uri asteapta evenimentul io
    // Atunci cand gasesc un thread care asteapta io,
    // ii setez starea la READY
	for (int i = 0; i <= 5; i++) {
		temp = scheduler->queue[i];
		while (temp) {
			if (temp->thread_data->state == WAITING &&
					temp->thread_data->io == io) {
				temp->thread_data->state = READY;
				k++;
			}
			temp = temp->next;
		}
	}
    // Apelez so_exec pentru a consuma timp
    // pe procesor si pentru a verifica daca trebuie
    // preemptat
	so_exec();
	return k;
}

/*
 * does whatever operation
 */
DECL_PREFIX void so_exec(void)
{
	my_thread *running = scheduler->running;
	thread_node *head, *curr;
	my_thread *highest = get_highest_prio_thread();
	int ret;

    // Scad timpul pentru thread-ul care ruleaza
	running->time--;
    // Thread-ului i-a expirat cuanta de timp
	if (running->time == 0) {
		if (highest != NULL) {
			// Verific daca exista un alt thread care are prioritate
			// mai mare sau egala
			// In acest caz preemptez thread-ul curent
			// si lansez thread-ul gasit
			if (highest->prio >= running->prio) {
				head = scheduler->queue[running->prio];
				if (head->next != NULL) {
					curr = scheduler->queue[running->prio];
					while (curr->next != NULL)
						curr = curr->next;
					scheduler->queue[running->prio] = head->next;
					head->next = NULL;
					curr->next = head;
				}
				running->state = READY;
				running->time = scheduler->time_quantum;
				scheduler->running = highest;
				ret = sem_post(&highest->run);
				DIE(ret == -1, "sem_post");
				ret = sem_wait(&running->run);
				DIE(ret == -1, "sem_wait");
			} else {
				// Daca nu exista alt thread,
				// resetez timpul thread-ului
				running->time = scheduler->time_quantum;
			}
		}
	} else {
		// In cazul in care nu s-a terminat
		// timpul thread-ului curent, verific daca a intrat in
		// sistem un thread cu prioritate mai mare
		// In acest caz, preemptez thread-ul curent
		if (highest != NULL) {
			if (highest->prio > running->prio) {
				head = scheduler->queue[running->prio];
				if (head->next != NULL) {
					curr = scheduler->queue[running->prio];
					while (curr->next != NULL)
						curr = curr->next;
					scheduler->queue[running->prio] = head->next;
					head->next = NULL;
					curr->next = head;
				}
				running->state = READY;
				running->time = scheduler->time_quantum;
				scheduler->running = highest;
				sem_post(&highest->run);
				ret = sem_wait(&running->run);
				DIE(ret == -1, "sem_wait");
			}
		}
	}
}

/*
 * destroys a scheduler
 */
DECL_PREFIX void so_end(void)
{
	thread_node *node = NULL, *aux;

	if (scheduler != NULL) {
		// Asteptarea threduri-lor
		if (scheduler->tlist != NULL)
			sem_wait(&scheduler->finish);

		node = scheduler->tlist;
		while (node) {
			pthread_join(node->thread_data->thread, NULL);
			node = node->next;
		}

		// Eliberarea memoriei
		for (int i = 0; i <= 5; i++) {
			node = scheduler->queue[i];
			while (node) {
				aux = node;
				node = node->next;
				free(aux);
			}
		}
		free(scheduler->queue);

		node = scheduler->tlist;
		while (node) {
			aux = node;
			node = node->next;
			free(aux->thread_data);
			sem_destroy(&aux->thread_data->run);
			sem_destroy(&aux->thread_data->ready);
			free(aux);
		}
	}
	sem_destroy(&scheduler->finish);
	free(scheduler);
	scheduler = NULL;
}
