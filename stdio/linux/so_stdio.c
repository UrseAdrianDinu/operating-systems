#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "so_stdio.h"
#define BUFLEN 4096
#define PIPE_READ	0
#define PIPE_WRITE	1

struct _so_file {
	int fd;
	char *buffer;
	int flag;
	int bufpos;
	long cursor;
	int op;
	int eoflag;
	int error;
	int bytes_read;
	int pid;
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int fd;
	SO_FILE *so_file;

	/* Alocare memorie structura */
	so_file = malloc(sizeof(SO_FILE));

	if (so_file == NULL)
		exit(12);

	so_file->buffer = malloc(BUFLEN * sizeof(char));

	if (so_file->buffer == NULL)
		exit(12);

	/* Setare mod de acces si deschidere fisier */
	if (!strcmp(mode, "r"))
		so_file->flag = O_RDONLY;
	else if (!strcmp(mode, "r+"))
		so_file->flag = O_RDWR;
	else if (!strcmp(mode, "w"))
		so_file->flag = O_WRONLY | O_TRUNC | O_CREAT;
	else if (!strcmp(mode, "w+"))
		so_file->flag = O_RDWR | O_TRUNC | O_CREAT;
	else if (!strcmp(mode, "a"))
		so_file->flag = O_APPEND | O_WRONLY | O_CREAT;
	else if (!strcmp(mode, "a+"))
		so_file->flag = O_APPEND | O_RDWR | O_CREAT;
	else {
		free(so_file->buffer);
		free(so_file);
		return NULL;
	}

	fd = open(pathname, so_file->flag, 0644);
	if (fd < 0) {
		free(so_file->buffer);
		free(so_file);
		return NULL;
	}

	/* Initializare campuri structura */
	so_file->fd = fd;
	so_file->bufpos = 0;
	so_file->cursor = 0;
	so_file->eoflag = 0;
	so_file->error = 0;
	so_file->bytes_read = 0;
	so_file->op = 0;
	return so_file;
}

int so_fclose(SO_FILE *stream)
{
	int r;

	/* FFlush pt. a scrie ce a ramas in buffer */
	r = so_fflush(stream);
	if (r < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	/* Inchid file descriptorul asociat structurii  */
	r = close(stream->fd);
	if (r < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	/* Eliberez memoria */
	free(stream->buffer);
	free(stream);
	return 0;
}


int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
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
	r = lseek(stream->fd, offset, whence);
	if (r == -1)
		return -1;
	stream->cursor = r;
	return 0;
}

long so_ftell(SO_FILE *stream)
{
	return stream->cursor;
}

int so_fgetc(SO_FILE *stream)
{
	ssize_t r;

	if (stream == NULL)
		return SO_EOF;

	/* Setez ultima operatie la una de citire */
	stream->op = 1;
	/* Daca pozitia in buffer este 0 sau am citit tot din buffer, */
	/* incerc sa citesc in buffer 4096 bytes */
	if (stream->bufpos == 0 || stream->bytes_read == stream->bufpos) {
		memset(stream->buffer, 0, BUFLEN);
		r = read(stream->fd, stream->buffer, BUFLEN);
		/* In caz de eroare sau EOF, setez  */
		/* campurile eoflag sau error */
		if (r == 0) {
			stream->eoflag = 1;
			return SO_EOF;
		}
		if (r == -1) {
			stream->error = 1;
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


size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t bytes_read = 0;
	int ch;

	if (stream == NULL)
		return SO_EOF;

	/* Citesc nmemb elemente de dimensiune size */
	/* In caz de eroare, returnez 0 */
	/* In caz ca intalnesc EOF, dau break */
	while (bytes_read < size * nmemb) {
		ch = so_fgetc(stream);
		if (so_feof(stream))
			break;
		if (so_ferror(stream))
			return 0;
		*((char *)(ptr + bytes_read)) = ch;
		bytes_read++;
	}
	return bytes_read/size;
}

int so_fputc(int c, SO_FILE *stream)
{
	size_t bytes_written = 0;
	ssize_t bytes_written_now;

	if (stream == NULL)
		return SO_EOF;

	/* Setez ultima operatie la una de scriere */
	stream->op = 2;
	/* In cazul in care buffer-ul este plin, scriu tot continutul */
	/* acestuia in fisier */
	/* In caz de eroare, intorc SO_EOF */
	if (stream->bufpos == BUFLEN) {
		while (bytes_written < BUFLEN) {
			bytes_written_now = write(stream->fd, stream->buffer + bytes_written,
					BUFLEN - bytes_written);
			if (bytes_written_now == -1) {
				stream->error = 1;
				return SO_EOF;
			}
			bytes_written += bytes_written_now;
		}
		/* Setez pozitia in buffer la 0 */
		/* Zeroizez buffer-ul */
		stream->bufpos = 0;
		memset(stream->buffer, 0, BUFLEN);
	}

	stream->buffer[stream->bufpos] = c;
	stream->cursor++;
	return stream->buffer[stream->bufpos++];
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t bytes_written = 0;
	int ch;

	/* Scriu nmemb elemente de dimensiune size */
	/* In caz de eroare, returnez 0 */
	while (bytes_written < size * nmemb) {
		ch = so_fputc(*((char *)(ptr + bytes_written)), stream);
		if (so_ferror(stream)) {
			stream->error = 1;
			return 0;
		}
		bytes_written++;
	}
	return bytes_written / size;
}


int so_fflush(SO_FILE *stream)
{
	size_t bytes_written = 0;
	ssize_t bytes_written_now;

	/* Daca ultima operatie nu a fost una */
	/* de scriere, intorc 0 */
	if (stream->op != 2)
		return 0;

	/* Scriu ce a ramas in buffer  */
	while (bytes_written < stream->bufpos) {
		bytes_written_now = write(stream->fd, stream->buffer + bytes_written,
				stream->bufpos - bytes_written);
		if (bytes_written_now == -1) {
			stream->error = 1;
			return SO_EOF;
		}
		bytes_written += bytes_written_now;
	}

	/* Zeroizez buffer-ul si setez pozitia */
	/* in buffer la 0 */
	memset(stream->buffer, 0, BUFLEN);
	stream->bufpos = 0;
	return 0;
}

int so_feof(SO_FILE *stream)
{
	return stream->eoflag;
}

int so_ferror(SO_FILE *stream)
{
	return stream->error;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	int r;
	SO_FILE *so_file = NULL;
	pid_t pid;
	int fds[2];

	/* Creez un pipe intre capatul de citire si capatul de scriere */
	r = pipe(fds);
	if (r != 0)
		return NULL;

	/* Aloc memorie pentru structura noua */
	so_file = malloc(sizeof(SO_FILE));

	if (so_file == NULL) {
		close(fds[0]);
		close(fds[1]);
		return NULL;
	}

	/* Creez procesul copil */
	pid = fork();

	switch (pid) {
	/* eroare */
	case -1:
			close(fds[0]);
			close(fds[1]);
			free(so_file);
			return NULL;
	/* procesul copil */
	case 0:
			if (strcmp(type, "r") == 0) {
				/* Inchid capatul nefolosit */
				r = close(fds[PIPE_READ]);
				if (r < 0)
					return NULL;
				/* Redirectez STDOUT la capatul de scriere */
				r = dup2(fds[PIPE_WRITE], STDOUT_FILENO);
				if (r < 0)
					return NULL;
			} else {
				/* Inchid capatul nefolosit */
				r = close(fds[PIPE_WRITE]);
				if (r < 0)
					return NULL;
				/* Redirectez STDIN la capatul de citire */
				r = dup2(fds[PIPE_READ], STDIN_FILENO);
				if (r < 0)
					return NULL;
			}
			/* Execut comanda */
			r = execl("/bin/sh", "sh", "-c", command, NULL);
			if (r < 0)
				return NULL;
			break;

	/* procesul parinte */
	default:
			if (strcmp(type, "r") == 0) {
				/* Setez fd-ul noii structurii */
				so_file->fd = fds[PIPE_READ];
				/* Inchid capatul nefolosit */
				r = close(fds[PIPE_WRITE]);
				if (r < 0)
					return NULL;
			} else {
				/* Setez fd-ul noii structurii */
				so_file->fd = fds[PIPE_WRITE];
				/* Inchid capatul nefolosit */
				r = close(fds[PIPE_READ]);
				if (r < 0)
					return NULL;
			}
			/* Setez campurile structurii create. */
			so_file->buffer = malloc(BUFLEN * sizeof(char));
			if (so_file->buffer == NULL)
				return NULL;
			so_file->bufpos = 0;
			so_file->cursor = 0;
			so_file->eoflag = 0;
			so_file->error = 0;
			so_file->bytes_read = 0;
			so_file->op = 0;
			so_file->pid = pid;
			break;
	}

	return so_file;
}

int so_pclose(SO_FILE *stream)
{
	int r = 0;
	int wstatus;

	/* FFlush pt. a scrie ce a ramas in buffer */
	r = so_fflush(stream);
	if (r < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	/* Inchid fisierul */
	r = close(stream->fd);
	if (r < 0) {
		free(stream->buffer);
		free(stream);
		return SO_EOF;
	}

	/* Astept terminarea procesului. */
	r = waitpid(stream->pid, &wstatus, 0);
	if (r == -1) {
		free(stream->buffer);
		free(stream);
		return -1;
	}

	/* Eliberez memoria */
	free(stream->buffer);
	free(stream);

	return wstatus;
}
