/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>

#include "exec_parser.h"
#include "utils.h"

static so_exec_t *exec;
static struct sigaction old_action;
static int pageSize;
static int fd;

static void sig_handler(int signum, siginfo_t *info, void *context)
{
	uintptr_t addr;
	int rc;
	int i;
	int ok = 0;
	int *data;
	char *p;
	int datasize = 0;
	int freespace = 0;
	int offset = 0;
	int current_page = 0;

	// Daca nu este un semnal segmentation fault,
	// atunci se apeleaza old action handler
	if (signum != SIGSEGV) {
		old_action.sa_sigaction(signum, info, context);
		return;
	}
	// Adresa care a generat page fault
	addr = (uintptr_t)info->si_addr;
	for (i = 0; i < exec->segments_no; i++) {
		// Verific daca se gaseste in segmentul curent
		if (addr >= exec->segments[i].vaddr &&
			addr <= exec->segments[i].vaddr + exec->segments[i].mem_size) {
			// Determin pagina din care face parte adresa
			ok = 1;
			current_page = (addr - exec->segments[i].vaddr) / pageSize;
			data = (int *)exec->segments[i].data;
			// Daca pagina curenta nu a fost mapata
			if (!data[current_page]) {
				// Mapez pagina la adresa corespunzatoare
				// Folosind flag-ul MAP_ANONYMOUS, continutul ei
				// este initializat cu 0
				p = mmap((void *)(current_page * pageSize + exec->segments[i].vaddr),
						 pageSize,
						 PROT_WRITE,
						 MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
						 -1,
						 0);

				DIE(p == MAP_FAILED, "mmap");

				// Calculez offset-ul din cadrul fisierului la care incepe pagina
				offset = current_page * pageSize + exec->segments[i].offset;
				// Verific daca pagina curenta nu depaseste file size
				if (current_page * pageSize < exec->segments[i].file_size) {
					rc = lseek(fd, offset, SEEK_SET);
					DIE(rc == -1, "lseek");
					// Pagina curenta este ultima din file size
					// iar adresa ei + dimensiunea unei pagini depaseste dimensiunea
					// segmentului in cadrul fisierului
					if ((current_page + 1) * pageSize > exec->segments[i].file_size) {
						freespace = (current_page + 1)*pageSize - exec->segments[i].file_size;
						datasize = pageSize - freespace;
					} else
						datasize = pageSize;
					// Citesc datele din fisier in pagina mapata
					rc = read(fd, p, datasize);
					DIE(rc == -1, "read");
				}
				// Setare pemisiuni
				rc = mprotect(p, pageSize, exec->segments[i].perm);
				DIE(rc == -1, "mmprotect");
				// Setez pagina curenta ca mapata
				data[current_page] = 1;
			} else {
				// Daca pagina curenta a fost mapata, atunci apelez
				// handler-ul default
				old_action.sa_sigaction(signum, info, context);
				return;
			}
			break;
		}
	}
	// In cazul in care adresa care a generat seg fault
	// nu este intr-un segment cunoscut, apelez handler-ul default
	if (!ok) {
		old_action.sa_sigaction(signum, info, context);
		return;
	}
}

int so_init_loader(void)
{
	/* TODO: initialize on-demand loader */
	struct sigaction action;
	int rc;

	// Determin dimensiunea unei pagini
	pageSize = getpagesize();

	// Setare handler
	action.sa_sigaction = sig_handler;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGSEGV);
	action.sa_flags = SA_SIGINFO;
	// Handle SIGSEGV
	rc = sigaction(SIGSEGV, &action, &old_action);
	DIE(rc == -1, "sigaction");
	return -1;
}

// Functie care initializeaza campul data
// pentru fiecare segment
void init_seg_data(void)
{
	unsigned int mem_size;
	int i;
	int noPages = 0;

	for (i = 0; i < exec->segments_no; i++) {
		mem_size = exec->segments[i].mem_size;
		noPages = (int)ceil((float)mem_size / pageSize);
		exec->segments[i].data = calloc(noPages + 1, sizeof(int));
	}
}

int so_execute(char *path, char *argv[])
{

	exec = so_parse_exec(path);
	if (!exec)
		return -1;
	// Deschid executabilul
	fd = open(path, O_RDONLY, 0644);
	DIE(fd < 0, "open");
	// Am folosit campul data al segmentelor, pentru
	// a retine paginile mapate
	init_seg_data();

	so_start_exec(exec, argv);

	return -1;
}
