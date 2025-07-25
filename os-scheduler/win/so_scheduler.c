#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "so_scheduler.h"

// Starile unui thread
enum state
{
	NEW,
	READY,
	RUNNING,
	WAITING,
	TERMINATED
};

/* Structura ce reprezinta un thread */
typedef struct my_thread {
	unsigned int prio;
	unsigned int time;

	int io;
	HANDLE thread;
	tid_t id;
	enum state state;
	so_handler *func;
	HANDLE ready;
	HANDLE run;
} my_thread;

/* Structura ce reprezinta un nod din lista */
typedef struct thread_node {
	my_thread *data;
	struct thread_node *next;
} thread_node;

/* Structura ce reprezinta scheduler-ul */
typedef struct my_scheduler {
	my_thread *running;

	thread_node **queue;
	thread_node *tlist;
	HANDLE  finish;
	unsigned int time_quantum;
	unsigned int io;
	int ready;
} my_scheduler;

static my_scheduler *scheduler;

/* Functie care insereaza un thread */
/* in coada de prioritati asociata */
void enqueue_prio_queue(my_thread *thread)
{
	thread_node *curr;
	thread_node *node = malloc(sizeof(thread_node));

	if (node == NULL)
		exit(EXIT_FAILURE);

	node->data = thread;
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

/* Functie care insereaza un thread */
/* in lista de thread-uri */
void enqueue_threads_list(my_thread *thread)
{
	thread_node *node = malloc(sizeof(thread_node));

	if (node == NULL)
		exit(EXIT_FAILURE);
	node->data = thread;
	if (scheduler->tlist == NULL) {
		node->next = NULL;
		scheduler->tlist = node;
	} else {
		node->next = scheduler->tlist;
		scheduler->tlist = node;
	}
}

/* Functie care scoate un thread din coada */
void dequeue(thread_node **queue, my_thread *t)
{
	thread_node *head = *queue;
	thread_node *aux = NULL;
	
	if (head != NULL) {
		if (head->data->thread == t->thread) {
			*queue = (*queue)->next;
			free(head);
		} else {
			if (head->next != NULL) {
				while (head->next->data->thread != t->thread)
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

/* Functie care intoarce thread-ul care are */
/* cea mai mare prioritate */
my_thread *get_highest_prio_thread(void)
{
	int i;
	my_thread *running;
	thread_node *temp;

	running = scheduler->running;
	for (i = 5; i >= 0; i--) {
		temp = scheduler->queue[i];
		while (temp) {
			if (temp->data->state != WAITING &&
					temp->data->id != running->id)
				return temp->data;
			temp = temp->next;
		}
	}
	return NULL;
}

DECL_PREFIX int so_init(unsigned int time_quantum, unsigned int io)
{
	int i;

    /* Verificarea parametrilor */
	if (time_quantum <= 0 || io > 256)
		return -1;

    /* Verificare daca scheduler-ul */
    /* a mai fost initializat */
	if (scheduler != NULL)
		if (scheduler->ready == 1)
			return -1;

    /* Initializare scheduler */

	scheduler = malloc(sizeof(my_scheduler));

	if (scheduler == NULL)
		exit(EXIT_FAILURE);

	scheduler->ready = 1;
	scheduler->io = io;
	scheduler->time_quantum = time_quantum;
	scheduler->queue = malloc(6 * sizeof(struct thread_node));
	if (scheduler->queue == NULL)
		exit(EXIT_FAILURE);
	for (i = 0; i <= 5; i++)
		scheduler->queue[i] = NULL;

	scheduler->running = NULL;
	scheduler->tlist = NULL;
	scheduler->finish = CreateSemaphore(NULL, 0, 1, NULL);
	if (scheduler->finish  == NULL)
		exit(EXIT_FAILURE);

	return 0;
}

/* Functie care deblocheaza thread-ul care */
/* are cea mai mare prioritate daca exista */
/* in caz contrar deblocheaza semaforul finish */
void run_next_thread(void)
{
	my_thread *t = get_highest_prio_thread();
	DWORD dwRet;

	if (t != NULL) {
		scheduler->running = t;
		dwRet = ReleaseSemaphore(t->run, 1, NULL);
		if (dwRet == 0)
			exit(EXIT_FAILURE);
	} else {
		dwRet = ReleaseSemaphore(scheduler->finish, 1, NULL);
		if (dwRet == 0)
			exit(EXIT_FAILURE);
	}
}

/* Functia executata de fiecare thread */
void *start_thread(void *args)
{
	my_thread *thread = (struct my_thread *)args;
	DWORD dwRet;

    /* Inserare thread in lista de thead-uri */
    /* si in coada de prioritati */
	enqueue_prio_queue(thread);
	enqueue_threads_list(thread);
    /* Anunta scheduler-ul ca thread-ul este ready */
	dwRet = ReleaseSemaphore(thread->ready, 1, NULL);
	if (dwRet == 0)
		exit(EXIT_FAILURE);
	thread->state = READY;
     /* Asteapta sa fie deblocat */
	dwRet = WaitForSingleObject(thread->run, INFINITE);
	if (dwRet == WAIT_FAILED)
		exit(EXIT_FAILURE);
	thread->state = RUNNING;
    /* Ruleaza handler-ul */
	thread->func(thread->prio);
    /* Thread-ul este scos din coada */
	dequeue(&scheduler->queue[thread->prio], thread);
	thread->state = TERMINATED;

    /* Ruleaza urmatorul thread */
	run_next_thread();
	return NULL;
}

DECL_PREFIX tid_t so_fork(so_handler *func, unsigned int priority)
{
	DWORD dwRet;
	struct my_thread *mthread;
	tid_t threadId;

	/* Verificarea parametrilor */
	if (priority > SO_MAX_PRIO || func == NULL)
		return INVALID_TID;

	/* Initializare structura thread */
	mthread = malloc(sizeof(my_thread));

	if (mthread == NULL)
		exit(EXIT_FAILURE);
	mthread->prio = priority;
	mthread->func = func;
	mthread->state = NEW;
	mthread->time = scheduler->time_quantum;
	mthread->io = -1;
	mthread->ready = CreateSemaphore(NULL, 0, 1, NULL);
	if (mthread->ready == NULL)
		exit(EXIT_FAILURE);
	mthread->run = CreateSemaphore(NULL, 0, 1, NULL);
	if (mthread->ready == NULL)
		exit(EXIT_FAILURE);
	/* Creare thread */
	mthread->thread = CreateThread(NULL,
			0,
			(LPTHREAD_START_ROUTINE) start_thread,
			mthread,
			0,
			&threadId);
	if (mthread->thread == NULL)
		exit(EXIT_FAILURE);
	mthread->id = threadId;

	/* Asteapta thread-ul sa intre in starea READY */
	dwRet = WaitForSingleObject(mthread->ready, INFINITE);
	if (dwRet == WAIT_FAILED)
		exit(EXIT_FAILURE);

	/* In cazul in care nu exista niciun thread */
	/* care ruleaza, deblochez thread-ul curent */
	/* Altfel, apelez so_exec pentru a consuma timp */
	/* pe procesor si pentru a verifica daca trebuie preemptat */
	if (scheduler->running == NULL) {
		scheduler->running = mthread;
		dwRet = ReleaseSemaphore(mthread->run, 1, NULL);
		if (dwRet == 0)
			exit(EXIT_FAILURE);
	} else
		so_exec();
	return threadId;
}

DECL_PREFIX int so_wait(unsigned int io)
{
	my_thread *running;
	my_thread *t;
	DWORD dwRet;

	/* Verificare parametrilor */
	if (io < 0 || io >= scheduler->io)
		return -1;

	running = scheduler->running;
    /* Setez starea thread-ului care ruleaza */
    /* la WAITING, ii resetez timpul */
    /* si ii setez operatia io la care asteapta */
	running->state = WAITING;
	running->time = scheduler->time_quantum;
	running->io = io;
    /* Pornesc urmatorul thread  */
	t = get_highest_prio_thread();
	if (t != NULL) {
		scheduler->running = t;
		dwRet = ReleaseSemaphore(t->run, 1, NULL);
		if (dwRet == 0)
			exit(EXIT_FAILURE);
	}
    /* Thread-ul running asteapta  */
    /* pana cand este deblocat  */
	dwRet = WaitForSingleObject(running->run, INFINITE);
	if (dwRet == WAIT_FAILED)
		exit(EXIT_FAILURE);
	return 0;
}

DECL_PREFIX int so_signal(unsigned int io)
{
	int k, i;
	thread_node *temp;
    /* Verificarea parametrilor  */
	if (io < 0 || io >= scheduler->io)
		return -1;

	k = 0;
    /* Parcurg cozile de prioritati  */
    /* Numar cate thread-uri asteapta evenimentul io  */
    /* Atunci cand gasesc un thread care asteapta io,  */
    /* ii setez starea la READY  */
	for (i = 0; i <= 5; i++) {
		temp = scheduler->queue[i];
		while (temp) {
			if (temp->data->state == WAITING &&
					temp->data->io == io) {
				temp->data->state = READY;
				k++;
			}
			temp = temp->next;
		}
	}
    /* Apelez so_exec pentru a consuma timp  */
    /* pe procesor si pentru a verifica daca trebuie  */
    /* preemptat  */
	so_exec();
	return k;
}

DECL_PREFIX void so_exec(void)
{
	my_thread *run = scheduler->running;
	thread_node *head, *curr, *aux;
	my_thread *h = get_highest_prio_thread();
	DWORD dwRet;
	unsigned int p;

    /* Scad timpul pentru thread-ul care ruleaza  */
	run->time--;
    /* Thread-ului i-a expirat cuanta de timp  */
	if (run->time == 0) {
		if (h != NULL) {
			/* Verific daca exista un alt thread  */
			/* care are prioritate  */
			/* mai mare sau egala  */
			/* In acest caz preemptez thread-ul curent  */
			/* si lansez thread-ul gasit  */
			if (h->prio >= run->prio) {
				head = scheduler->queue[run->prio];
				if (head->next != NULL) {
					curr = scheduler->queue[run->prio];
					while (curr->next != NULL)
						curr = curr->next;
					p = run->prio;
					scheduler->queue[p] = head->next;
					head->next = NULL;
					curr->next = head;
				}
				run->state = READY;
				run->time = scheduler->time_quantum;
				scheduler->running = h;
				dwRet = ReleaseSemaphore(h->run, 1, NULL);
				if (dwRet == 0)
					exit(EXIT_FAILURE);
				WaitForSingleObject(run->run, INFINITE);
			} else {
				/* Daca nu exista alt thread,  */
				/* resetez timpul thread-ului  */
				run->time = scheduler->time_quantum;
			}
		}
	} else {
		/* In cazul in care nu s-a terminat  */
		/* timpul thread-ului curent, verific daca a intrat in  */
		/* sistem un thread cu prioritate mai mare  */
		/* In acest caz, preemptez thread-ul curent  */
		if (h != NULL) {
			if (h->prio > run->prio) {
				head = scheduler->queue[run->prio];
				if (head->next != NULL) {
					curr = scheduler->queue[run->prio];
					while (curr->next != NULL)
						curr = curr->next;
					aux = head->next;
					scheduler->queue[run->prio] = aux;
					head->next = NULL;
					curr->next = head;
				}
				run->state = READY;
				run->time = scheduler->time_quantum;
				scheduler->running = h;
				dwRet = ReleaseSemaphore(h->run, 1, NULL);
				if (dwRet == 0)
					exit(EXIT_FAILURE);
				WaitForSingleObject(run->run, INFINITE);
			}
		}
	}
}

DECL_PREFIX void so_end(void)
{
	thread_node *node, *aux;
	int i;

	if (scheduler != NULL) {
		/* Asteptarea threduri-lor  */
		if (scheduler->tlist != NULL)
			WaitForSingleObject(scheduler->finish, INFINITE);

		node = scheduler->tlist;
		while (node) {
			WaitForSingleObject(node->data->thread, INFINITE);
			node = node->next;
		}

		/* Eliberarea memoriei  */
		for (i = 0; i <= 5; i++) {
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
			free(aux->data);
			free(aux);
		}
	}
	free(scheduler);
	scheduler = NULL;

}

