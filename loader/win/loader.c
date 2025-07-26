#include <stdio.h>
#include <Windows.h>
#include <math.h>

#define DLL_EXPORTS
#include "loader.h"
#include "exec_parser.h"
#include "utils.h"

static so_exec_t *exec;
static int pageSize;
static LPVOID handler;
static HANDLE hFile;

static LONG CALLBACK access_violation(PEXCEPTION_POINTERS ExceptionInfo)
{
	DWORD old, rc;
	DWORD r;
	BOOL bRet;
	LPBYTE addr;
	int i;
	int *data;
	int ok = 0;
	int page = 0;
	char *p;
	int page_offset = 0;
	int datasize = 0;
	int freespace = 0;
	DWORD perm = 0;
	int bss = 0;
	int seg_perm = 0;
	uintptr_t addr_u;
	uintptr_t vaddr;
	unsigned int mem_size;
	unsigned int file_size;
	int page_start = 0;
	unsigned int seg_offset;
	int next;

	// Adresa care a generat page fault
	addr = (LPBYTE)ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
	addr_u = (uintptr_t)addr;
	for (i = 0; i < exec->segments_no; i++) {
		// Verific daca se gaseste in segmentul curent
		vaddr = exec->segments[i].vaddr;
		mem_size = exec->segments[i].mem_size;
		file_size = exec->segments[i].file_size;
		if (addr_u >= vaddr &&
			addr_u <= vaddr + mem_size) {
			// Determin pagina din care face parte adresa
			page = (int)(addr_u - vaddr) / pageSize;
			ok = 1;
			data = (int *)exec->segments[i].data;
			// Daca pagina curenta nu a fost mapata
			if (!data[page]) {
				// Mapez pagina la adresa corespunzatoare
				page_start = vaddr + page*pageSize;
				p = VirtualAlloc((void *)(page_start),
					pageSize,
					MEM_COMMIT | MEM_RESERVE,
					PAGE_READWRITE);
				DIE(p == NULL, "VirtualAlloc");
				// Calculez offset-ul din cadrul fisierului la
				// care incepe pagina
				seg_offset = exec->segments[i].offset;
				page_offset = page * pageSize + seg_offset;
				if ((unsigned int)page * pageSize < file_size) {
					rc = SetFilePointer(hFile,
							page_offset,
							NULL,
							SEEK_SET);
					if (rc == INVALID_SET_FILE_POINTER)
						exit(EXIT_FAILURE);
					// Verific daca pagina curenta
					// este ultima in fisier,
					// iar adresa ei de start +
					// dimensiunea unei pagini >
					// dimensiunea fisierului
					// In acest caz, determin cate date sunt
					// pana la file_size
					next = (page + 1) * pageSize;
					if ((unsigned int)next > file_size) {
						freespace = next - file_size;
						datasize = pageSize - freespace;
						bss = 1;
					} else
						datasize = pageSize;
					// Citesc din fisier datele
					// in memoria mapata
					bRet = ReadFile(hFile, p, datasize,
									&r,
									NULL);
					DIE(bRet == FALSE, "ReadFile");
				}
				// In cazul in care pagina
				// depaseste file_size,
				// zeroizez diferenta intre spatiul
				// din memorie si spatiul din fisier
				if (bss)
					memset((void *)(p + datasize),
							0,
							freespace);

				// Determin permisiunile segmentului
				seg_perm = exec->segments[i].perm;
				if (seg_perm & 1 &&
					!(seg_perm & 2) &&
					!(seg_perm & 4))
					perm = PAGE_READONLY;
				else if (!(seg_perm & 1) &&
					(seg_perm & 2) &&
					!(seg_perm & 4))
					perm = PAGE_READWRITE;
				else if (!(seg_perm & 1) &&
					!(seg_perm & 2) &&
					(seg_perm & 4))
					perm = PAGE_EXECUTE;
				else if ((seg_perm & 1) &&
					(seg_perm & 2) &&
					!(seg_perm & 4))
					perm = PAGE_READWRITE;
				else if ((seg_perm & 1) &&
					!(seg_perm & 2) &&
					(seg_perm & 4))
					perm = PAGE_EXECUTE_READ;
				else if (!(seg_perm & 1) &&
					(seg_perm & 2) &&
					(seg_perm & 4))
					perm = PAGE_EXECUTE_READWRITE;
				else if ((seg_perm & 1) &&
					(seg_perm & 2) &&
					(seg_perm & 4))
					perm = PAGE_EXECUTE_READWRITE;

				// Setez permisiunile
				rc = VirtualProtect(p, pageSize, perm, &old);
				DIE(!rc, "VirtualProtect");
				// Setez pagina curenta ca mapata
				data[page] = 1;
				break;
			}
			// Pagina curenta a fost mapata
			ok = 0;
			break;
		}
	}
	// Adresa care a generat seg fault, nu este intr-un
	// segment cunoscut sau pagina gasita a fost deja mapata
	if (!ok)
		return EXCEPTION_CONTINUE_SEARCH;

	return EXCEPTION_CONTINUE_EXECUTION;
}

int so_init_loader(void)
{

	// Setez dimensiunea unei pagini
	pageSize = 65536;
	// Setare handler
	handler = AddVectoredExceptionHandler(1, access_violation);
	DIE(handler == NULL, "ExceptionHandler");
	return -1;
}

// Functie care deschide un fisier
HANDLE open(const char *filename, DWORD flag)
{
	HANDLE hFile;

	hFile = CreateFile(
		filename,
		FILE_READ_DATA | FILE_WRITE_DATA,
		FILE_SHARE_READ,
		NULL,
		flag,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	DIE(hFile == INVALID_HANDLE_VALUE, "CreateFile");
	return hFile;
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
	hFile = open(path, OPEN_EXISTING);

	// Am folosit campul data al segmentelor, pentru
	// a retine paginile mapate
	init_seg_data();
	so_start_exec(exec, argv);

	return -1;
}
