#define DLL_EXPORTS
#include "so_stdio.h"
#define BUFLEN 4096
#define PIPE_READ 0
#define PIPE_WRITE 1

struct _so_file {
	HANDLE fd;
	char *buffer;
	DWORD flag;
	int bufpos;
	long cursor;
	int op;
	int eoflag;
	int error;
	int bytes_read;
	HANDLE pid;
	HANDLE hThread;
};
FUNC_DECL_PREFIX SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int fd;
	SO_FILE *so_file;
	DWORD share;
	/* Alocare memorie structura */
	so_file = malloc(sizeof(SO_FILE));

	if (so_file == NULL)
		exit(12);

	so_file->buffer = malloc(BUFLEN * sizeof(char));

	if (so_file->buffer == NULL)
		exit(12);
	/* Setare mod de acces si deschidere fisier */
	if (!strcmp(mode, "r")) {
		so_file->flag = GENERIC_READ|GENERIC_WRITE;
		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
		so_file->fd = CreateFile(pathname,
							so_file->flag,
							share,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
	} else if (!strcmp(mode, "r+")) {
		so_file->flag = GENERIC_READ | GENERIC_WRITE;
		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
		so_file->fd = CreateFile(pathname,
							so_file->flag,
							share,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
	} else if (!strcmp(mode, "w")) {
		so_file->flag = GENERIC_READ|GENERIC_WRITE;
		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
		so_file->fd = CreateFile(pathname,
							so_file->flag,
							share,
							NULL,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
	} else if (!strcmp(mode, "w+")) {
		so_file->flag = GENERIC_READ | GENERIC_WRITE;
		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
		so_file->fd = CreateFile(pathname,
							so_file->flag,
							share,
							NULL,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
	} else if (!strcmp(mode, "a")) {
		so_file->flag = FILE_APPEND_DATA;
		so_file->fd = CreateFile(pathname,
							so_file->flag,
							FILE_SHARE_WRITE,
							NULL,
							OPEN_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
	} else if (!strcmp(mode, "a+")) {
		so_file->flag = FILE_APPEND_DATA|FILE_READ_DATA;
		share = FILE_SHARE_READ|FILE_SHARE_WRITE;
		so_file->fd = CreateFile(pathname,
							so_file->flag,
							share,
							NULL,
							OPEN_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
	} else {
		free(so_file->buffer);
		free(so_file);
		return NULL;
	}

	if (so_file->fd == INVALID_HANDLE_VALUE) {
		free(so_file->buffer);
		free(so_file);
		return NULL;
	}
	/* Initializare campuri structura */
	so_file->bufpos = 0;
	so_file->cursor = 0;
	so_file->eoflag = 0;
	so_file->error = 0;
	so_file->bytes_read = 0;
	so_file->op = 0;
	return so_file;
}
FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
	int r;
	/* FFlush pt. a scrie ce a ramas in buffer */
	r = so_fflush(stream);
	if (r < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}
	/* Inchid handle-ul asociat structurii  */
	r = CloseHandle(stream->fd);
	if (r == FALSE) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}
	/* Eliberez memoria */
	free(stream->buffer);
	free(stream);
	return 0;

}
FUNC_DECL_PREFIX HANDLE so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

FUNC_DECL_PREFIX int so_fflush(SO_FILE *stream)
{
	BOOL bRet;
	DWORD bytesWritten = 0;
	DWORD bytesWrittenNow;

	/* Daca ultima operatie nu a fost una */
	/* de scriere, intorc 0 */
	if (stream->op != 2)
		return 0;
	
	/* Scriu ce a ramas in buffer  */
	while (bytesWritten < stream->bufpos) {
		bRet = WriteFile(stream->fd,
			stream->buffer + bytesWritten,
			stream->bufpos - bytesWritten,
			&bytesWrittenNow,
			NULL); 
		if (bRet == FALSE) {
			stream->error = 1;
			return SO_EOF;
		}
		bytesWritten += bytesWrittenNow;
	}

	/* Zeroizez buffer-ul si setez pozitia */
	/* in buffer la 0 */
	memset(stream->buffer, 0, BUFLEN);
	stream->bufpos = 0;
	return 0;
}

FUNC_DECL_PREFIX int so_fseek(SO_FILE *stream, long offset, int whence)
{
	DWORD cursor;
	int r;
	
	/* Daca ultima operatie a fost una de citire, */
	/* atunci invalidez buffer-ul */
	/* si setez pozitia in buffer 0. */

	/* Daca ultima operatie a fost una de scriere, */
	/* apelez so_fflush pentru a scrie in fisier */
	/* continutul buffer-ului. */
	if (stream->op == 1) {
		memset(stream->buffer, 0, BUFLEN);
		stream->bufpos = 0;
	} else if (stream->op == 2) {
		r = so_fflush(stream);
		if (r == -1)
			return -1;
	}
	/* Setez pozitia in fisier */
	cursor = SetFilePointer(stream->fd,
							offset,
							NULL,
							whence);
	if (cursor == INVALID_SET_FILE_POINTER)
		return SO_EOF;
	stream->cursor = cursor;
	return 0;
}
FUNC_DECL_PREFIX long so_ftell(SO_FILE *stream)
{
	return stream->cursor;
}

FUNC_DECL_PREFIX int so_fgetc(SO_FILE *stream)
{
	DWORD r;
	BOOL bRet;
	DWORD error;

	/* Setez ultima operatie la una de citire */
	stream->op = 1;
	/* Daca pozitia in buffer este 0 sau am citit tot din buffer, */
	/* incerc sa citesc in buffer 4096 bytes */
	if (stream->bufpos == 0 || stream->bytes_read == stream->bufpos) {
		memset(stream->buffer, 0, BUFLEN);
		bRet = ReadFile(stream->fd,
				stream->buffer,
				BUFLEN,
				&r,
				NULL);
		/* In caz de eroare sau EOF, setez  */
		/* campurile eoflag sau error */
		if (bRet == FALSE) {
			error = GetLastError();
			if (error == ERROR_BROKEN_PIPE) {
				stream->eoflag = 1;
				return SO_EOF;
			}
			stream->error = 1;
		}
		
		if (r == 0) {
			stream->eoflag = 1;
			return SO_EOF;
		}
		/* Setez pozitia in buffer la 0 */
		/* Setez numraul de bytes cititi la r */
		stream->bufpos = 0;
		stream->bytes_read = r;
	}
	/* Incrementez pozitia in buffer si in fisier */
	stream->cursor++;
	return (int)stream->buffer[stream->bufpos++];
}

FUNC_DECL_PREFIX
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int ch;
	size_t bytes_read = 0;
	
	
	if (so_feof(stream))
		return 0;
	/* Citesc nmemb elemente de dimensiune size */
	/* In caz de eroare, returnez 0 */
	/* In caz ca intalnesc EOF, dau break  */
	while (bytes_read < size*nmemb) {
		ch = so_fgetc(stream);
		if (so_feof(stream))
			break;
		if (so_ferror(stream))
			return 0;
		*((char *)ptr + bytes_read) = ch;
		bytes_read++;
	}
	return bytes_read/size;
}

FUNC_DECL_PREFIX int so_fputc(int c, SO_FILE *stream)
{
	BOOL bRet;
	DWORD bytesWritten = 0;
	DWORD bytesWrittenNow;
	
	/* Setez ultima operatie la una de scriere */
	stream->op = 2;
	
	/* In cazul in care buffer-ul este plin, scriu tot continutul */
	/* acestuia in fisier */
	/* In caz de eroare, intorc SO_EOF */
	if (stream->bufpos == BUFLEN) {
		while (bytesWritten < BUFLEN) {
			bRet = WriteFile(stream->fd,
					stream->buffer + bytesWritten,
					BUFLEN - bytesWritten,
					&bytesWrittenNow,
					NULL);
			if (bRet == FALSE) {
				stream->error = 1;
				return SO_EOF;
			}
			bytesWritten += bytesWrittenNow;
		}
		/* Setez pozitia in buffer la 0 */
		/* Zeroizez buffer-ul */
		stream->bufpos = 0;
		memset(stream->buffer, 0, BUFLEN);
	}
	/* Pun pe pozitia curenta in buffer, caracterul dat ca parametru. */
	stream->buffer[stream->bufpos] = (char)c;
	stream->cursor++;
	return stream->buffer[stream->bufpos++];
	
}


FUNC_DECL_PREFIX
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int ch;
	size_t bytes_written = 0;
	char *tmp = ptr;
	/* Scriu nmemb elemente de dimensiune size */
	/* In caz de eroare, returnez 0 */
	while (bytes_written < size * nmemb) {
		ch = so_fputc(*(tmp + bytes_written), stream);
		if (so_ferror(stream)) {
			stream->error = 1;
			return 0;
		}
		bytes_written++;
	}
	return bytes_written / size;
}



FUNC_DECL_PREFIX int so_feof(SO_FILE *stream)
{
	return stream->eoflag;
}
FUNC_DECL_PREFIX int so_ferror(SO_FILE *stream)
{
	return stream->error;
}

FUNC_DECL_PREFIX SO_FILE *so_popen(const char *command, const char *type)
{
	BOOL bRes;
	HANDLE hRead, hWrite;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SO_FILE *so_file;
	int r;
	char *compcomm = NULL;

	/* Aloc memorie */
	so_file = malloc(sizeof(SO_FILE));
	if (so_file == NULL)
		return NULL;
	
	compcomm = malloc(256*sizeof(char));
	if (compcomm == NULL)
		return NULL;

	/* Construiesc comanda */
	sprintf(compcomm, "cmd /C %s", command);
	/* Zeroizez structurile SECURITY_ATTRIBUTES, STARTUPINFO */
	/* si PROCESS_INFORMATION */
	ZeroMemory(&sa, sizeof(sa));
	sa.bInheritHandle = TRUE;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	
	/* Creez un pipe intre capatul de citire si capatul de scriere */
	bRes = CreatePipe(&hRead, &hWrite, &sa, 0);
	if (bRes == FALSE) {
		free(so_file);
		free(compcomm);
		return NULL;
	}
	/* R - redirectez STDOUT-UL procesului */
	/* copil la capatul de scriere */
	/* W - redirectez STDIN-UL procesului */
	/* copil la capatul de citire */
	if (strcmp(type, "r") == 0) {
		si.cb = sizeof(si);
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		si.hStdOutput = hWrite;
		si.dwFlags |= STARTF_USESTDHANDLES;
		SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
	} else {
		si.cb = sizeof(si);
		si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdInput = hRead;
		si.dwFlags |= STARTF_USESTDHANDLES;
		SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);
	}
	/* Creez procesul */
	bRes = CreateProcess(
			NULL,
			compcomm,
			NULL,
			NULL,
			TRUE,
			0,
			NULL,
			NULL,
			&si,
			&pi);
	
	if (bRes == FALSE) {
		r = CloseHandle(hRead);
		if (r == FALSE) {
			free(so_file->buffer);
			free(so_file);
			return NULL;
		}
		r = CloseHandle(hWrite);
		if (r == FALSE) {
			free(so_file->buffer);
			free(so_file);
			return NULL;
		}
		free(so_file);
		free(compcomm);
		return NULL;
	}
	
	if (strcmp(type, "r") == 0) {
		r = CloseHandle(hWrite);
		if (r == FALSE) {
			free(so_file->buffer);
			free(so_file);
			return NULL;
		}
		so_file->fd = hRead;
	} else {
		r = CloseHandle(hRead);
		if (r == FALSE) {
			free(so_file->buffer);
			free(so_file);
			return NULL;
		}
		so_file->fd = hWrite;
	}
	/* Setez campurile structurii SO_FILE */
	so_file->buffer = malloc(BUFLEN * sizeof(char));
	if (so_file->buffer == NULL)
		return NULL;
	
	so_file->bufpos = 0;
	so_file->cursor = 0;
	so_file->eoflag = 0;
	so_file->error = 0;
	so_file->bytes_read = 0;
	so_file->op = 0;
	so_file->pid = pi.hProcess;
	so_file->hThread = pi.hThread;
	free(compcomm);
	return so_file;
	
}
FUNC_DECL_PREFIX int so_pclose(SO_FILE *stream)
{
	DWORD ret = 0;
	DWORD status;
	int r;
	
	/* FFlush pt. a scrie ce a ramas in buffer */
	r = so_fflush(stream);
	if (r < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	/* Inchid handle-ul asociat structurii */
	r = CloseHandle(stream->fd);
	if (r == FALSE) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	/* Astept terminarea procesului. */
	ret = WaitForSingleObject(stream->pid, INFINITE);
	
	if (ret == WAIT_FAILED) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}
	
	/* Determin codul de eriare cu care s-a terminat procesul. */
	r = GetExitCodeProcess(stream->pid, &status);
	if (r == FALSE) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}
	
	r = CloseHandle(stream->pid);
	if (r == FALSE) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}
	/* Inchid handle-ele asociate procesului (hProcess, hThread) */
	r = CloseHandle(stream->hThread);
	if (r == FALSE) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	/* Eliberez memoria */
	free(stream->buffer);
	free(stream);
	return status;
}
