#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "hashmap.h"
#include "queue.h"

// Functie care inlocuieste aparitiile lui key in str cu value
void replace(char *str, char *key, char *value, int start, int end)
{
	char temp[256] = "";
	int pos = 0;

	if (value != NULL) {
		// Linia curenta nu contine ""
		if (start == -1) {
			while (strstr(str + pos, key) != NULL) {
				pos += strstr(str + pos, key) - (str + pos);
				strncpy(temp, str, pos);
				strcat(temp, value);
				strcat(temp, str + pos + strlen(key));
				strcpy(str, temp);
				pos += strlen(value);
			}
		} else {
			// Linia curenta contine ""
			while (strstr(str + pos, key) != NULL) {
				pos += strstr(str + pos, key) - (str + pos);
				// Daca am gasit o aparitie, verific sa nu fie intre ""
				if (pos < start || pos > end) {
					strncpy(temp, str, pos);
					strcat(temp, value);
					strcat(temp, str + pos + strlen(key));
					strcpy(str, temp);
					pos += strlen(value);
				} else
					pos = end;
			}
		}
	}
}

// Functie care creeaza path-ul pentru fisierul header, pornind
// de la input_file
void createPath(char *input_file, char *header, char *hp)
{
	int i;

	strcpy(hp, input_file);
	i = strlen(input_file) - 1;
	while (input_file[i] != '/')
		i = i - 1;
	hp[i + 1] = '\0';
	strcat(hp, header);
}

// Functie care trateaza o linie de tipul #define <SYMBOL> <MAPPING>
// Adauga in hashmap intrarea <SYMBOL>=<MAPPING>
// Inlocuieste in MAPPING, simbolurile care au definite anterior
// Flag indica daca linia curenta trebuie procesata sau nu
void deffunc(HashMap *hashmap, char *buffer, int flag, FILE *input)
{
	char *tok;
	char *symbol;
	char *d = "\t []{}<>=+-*/%!&|^.,:;()\\";
	char *map;
	char *strpos;
	char cpy[256];
	int l = 0;
	int offset = 0;
	char *v;

	strcpy(cpy, buffer);

	if (flag == 1) {
		if (strstr(buffer, "\\\n") == NULL) {
			tok = strtok(buffer, d);
			symbol = strtok(NULL, d);
			map = strtok(NULL, d);
			if (map != NULL) {
				strpos = strstr(cpy, map);
				offset = strpos - cpy;
				strcpy(map, cpy + offset);
				strcpy(cpy, cpy + offset);
				tok = strtok(cpy, " ");
				while (tok) {
					l = strlen(tok);

					if (tok[l - 1] == '\n')
						tok[l - 1] = '\0';
					v = getV(hashmap, tok);
					if (v != NULL)
						replace(map, tok, v, -1, -1);
					tok = strtok(NULL, " ");
				}
				map[strlen(map) - 1] = '\0';
				put_entry(hashmap, symbol, map);
			} else {
				if (symbol[strlen(symbol) - 1] == '\n')
					symbol[strlen(symbol) - 1] = '\0';
				put_entry(hashmap, symbol, "");
			}
		} else {
			tok = strtok(buffer, d);
			tok = strtok(NULL, d);
			symbol = malloc((strlen(tok) + 1) * sizeof(char));
			if (symbol == NULL)
				exit(12);
			strcpy(symbol, tok);
			tok = strtok(NULL, d);
			map = malloc((strlen(tok) + 1) * sizeof(char));
			if (map == NULL)
				exit(12);
			strcpy(map, tok);
			fgets(buffer, 256, input);
			while (strstr(buffer, "\\\n") != NULL) {
				buffer[strlen(buffer) - 2] = '\0';
				l = strlen(map);
				l +=  strlen(buffer) + 1;
				map = realloc(map, l * sizeof(char));
				if (map == NULL)
					exit(12);
				strcat(map, buffer);
				fgets(buffer, 256, input);
			}
			l = strlen(map);
			l +=  strlen(buffer) + 1;
			buffer[strlen(buffer) - 1] = '\0';
			map = realloc(map, l * sizeof(char));
			if (map == NULL)
				exit(12);
			strcat(map, buffer);
			put_entry(hashmap, symbol, map);

			free(map);
			free(symbol);
		}
	}
}

// Functie care citeste linie cu linie fisierul de intrare,
// analizeaza fiecare linie si o
// afiseaza la output (daca este cazul) linia procesata
void readFile(FILE *input, FILE *output,
	HashMap *hashmap, Queue *queue, char *input_file_name)
{
	char buffer[256];
	char cpy[256];			// copie buffer
	char *tok;				// token pt strtok
	int depth = 0;			// nivel imbricare
	int if_cond[100];
	char *hname;			// nume header
	char *hp = NULL;		// path header
	FILE *header;
	int header_found = 0;	// flag care indica daca header-ul
							// a fost gasit
	Node *temp;
	char *position;
	char *d = "\t []{}<>=+-*/%!&|^.,:;()\\";
	int l = 0;
	char *v;
	int st, dr;		// folosite pt. liniile care au ""

	if_cond[0] = 1;
	while (fgets(buffer, 256, input))  {
		strcpy(cpy, buffer);
		if (strstr(buffer, "#define") == buffer) {
			printf("\n");
			deffunc(hashmap, buffer, if_cond[depth], input);
		} else if (strstr(buffer, "#undef") == buffer) {
			tok = strtok(buffer, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			tok = strtok(NULL, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			remove_entry(hashmap, tok);
		} else if (strstr(buffer, "#if ") == buffer) {
			tok = strtok(buffer, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			tok = strtok(NULL, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			if (tok[strlen(tok) - 1] == '\n')
				tok[strlen(tok) - 1] = '\0';
			if (containsKey(hashmap, tok) == 1)
				tok = getV(hashmap, tok);

			if (tok[0] == '0') {
				depth++;
				if_cond[depth] = 0;
			} else {
				depth++;
				if_cond[depth] = 1;
			}
		} else if (strstr(buffer, "#endif") == buffer) {
			depth--;

		} else if (strstr(buffer, "#else") == buffer) {
			if (if_cond[depth] == 1)
				if_cond[depth] = 0;
			else
				if_cond[depth] = 1;
		} else if (strstr(buffer, "#elif ") == buffer) {
			tok = strtok(buffer, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			tok = strtok(NULL, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			if (containsKey(hashmap, tok) == 1)
				tok = getV(hashmap, tok);

			if (tok[0] != '0' && if_cond[depth] == 0)
				if_cond[depth] = 1;
			else
				if_cond[depth] = 0;

		} else if (strstr(buffer, "#ifdef ") == buffer) {
			tok = strtok(buffer, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			tok = strtok(NULL, "\t\n []{}<>=+-*/%!&|^.,:;()\\");

			if (containsKey(hashmap, tok) == 1) {
				depth++;
				if_cond[depth] = 1;
			} else {
				depth++;
				if_cond[depth] = 0;
			}
		} else if (strstr(buffer, "#ifndef ") == buffer) {
			tok = strtok(buffer, "\t\n []{}<>=+-*/%!&|^.,:;()\\");
			tok = strtok(NULL, "\t\n []{}<>=+-*/%!&|^.,:;()\\");

			if (containsKey(hashmap, tok) != 1) {
				depth++;
				if_cond[depth] = 1;
			} else {
				depth++;
				if_cond[depth] = 0;
			}
		} else if (strstr(buffer, "#include") == buffer) {
			hname = buffer + 9;
			if (hname[0] == '"')
				hname++;
			hname[strlen(hname) - 2] = '\0';
			if (input_file_name != NULL) {
				l = strlen(hname);
				l +=  strlen(input_file_name) + 5;
				hp = malloc(l * sizeof(char));
				if (hp == NULL)
					exit(12);
			}

			// Se cauta in directorul fisierului
			// de input sau in directorul curent
			if (input_file_name == NULL) {
				l = strlen(hname) + 10;
				hp = malloc(l * sizeof(char));
				if (hp == NULL)
					exit(12);
				createPath("./", hname, hp);
			} else {
				createPath(input_file_name, hname, hp);
			}
			header = fopen(hp, "r");
			if (header != NULL)
				header_found = 1;
			// Daca nu a fost gasit se cauta
			// in directoarele date ca argument
			if (header_found == 0) {
				temp = queue->head;
				while (temp) {
					if (hp == NULL) {
						l = strlen(temp->filename);
						l +=  strlen(hname) + 5;
						hp = malloc(l * sizeof(char));
					if (hp == NULL)
						exit(12);
					}
					strcpy(hp, temp->filename);
					strcat(hp, "/");
					strcat(hp, hname);
					header = fopen(hp, "r");
					if (header != NULL) {
						header_found = 1;
						break;
					}
					temp = temp->next;
				}
			}

			if (header_found == 0) {
				free(hp);
				exit(1);
			}
			readFile(header, output,
				hashmap, queue, input_file_name);
			free(hp);
			fclose(header);

		} else if (if_cond[depth] == 1) {
			// Linia curenta nu contine ""
			if (strchr(buffer, '\"') == NULL) {
				tok = strtok(buffer, d);
				while (tok) {
					v = getV(hashmap, tok);
					if (v != NULL)
						replace(cpy, tok, v, -1, -1);
					tok = strtok(NULL, d);
				}
				cpy[strlen(cpy)] = '\0';
				fprintf(output, "%s", cpy);
			} else {
				// Linia curenta contine ""
				// In acest caz inlocuiesc cuvintele
				// care sunt dupa ""
				position = strchr(buffer, '\"');
				st = position - buffer;
				position = strchr(position + 1, '\"');
				dr = position - buffer;
				tok = strtok(position + 1, d);
				while (tok) {
					v = getV(hashmap, tok);
					if (v != NULL)
						replace(cpy, tok, v, st, dr);
					tok = strtok(NULL, d);
				}
				fprintf(output, "%s", cpy);
			}
		} else {
			printf("\n");
		}
	}
}


int main(int argc, char *argv[])
{
	HashMap *hashmap = init_hashmap(1000);
	Queue *queue = createQueue();
	FILE *input;
	FILE *output;
	char *input_file_name = NULL;
	char *output_file_name = NULL;
	int i;
	char *key;
	char *value;
	char *filename;
	int l = 0;

	// Analizare argumente
	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "-D", 2) == 0) {
			if (strlen(argv[i]) == 2) {
				i = i + 1;
				key = strtok(argv[i], "=");
				value = strtok(NULL, "=");
				if (value != NULL)
					put_entry(hashmap, key, value);
				else
					put_entry(hashmap, key, "");
			} else {
				key = strtok(argv[i] + 2, "=");
				value = strtok(NULL, "=");
				if (value != NULL)
					put_entry(hashmap, key, value);
				else
					put_entry(hashmap, key, "");
			}
	} else if (strncmp(argv[i], "-I", 2) == 0) {
		if (strlen(argv[i]) == 2) {
			i = i + 1;
			pushBack(queue, argv[i]);
		} else {
			filename = argv[i] + 2;
			pushBack(queue, filename);
		}
	} else if (strncmp(argv[i], "-o", 2) == 0) {
		if (strlen(argv[i]) == 2) {
			i = i + 1;
			l = strlen(argv[i]) + 1;
			output_file_name = malloc(l * sizeof(char));
			if (output_file_name == NULL)
				exit(12);
			strcpy(output_file_name, argv[i]);
		} else {
			l = strlen(argv[i]) + 1;
			output_file_name = malloc(l * sizeof(char));
			if (output_file_name == NULL)
				exit(12);
			strcpy(output_file_name, argv[i] + 2);
		}
	} else {
		if (input_file_name != NULL) {
			if (output_file_name != NULL)
				exit(12);
			l = strlen(argv[i]) + 1;
			output_file_name = malloc(l * sizeof(char));
			if (output_file_name == NULL)
				exit(12);
			strcpy(output_file_name, argv[i]);
		} else {
			l = strlen(argv[i]) + 1;
			input_file_name = malloc(l * sizeof(char));
			if (input_file_name == NULL)
				exit(12);
			strcpy(input_file_name, argv[i]);
		}
	}
	}

	if (input_file_name != NULL) {
		input = fopen(input_file_name, "r");
		if (input == NULL)
			return 1;
	} else
		input = stdin;

	if (output_file_name != NULL) {
		output = fopen(output_file_name, "w");
		if (output == NULL)
			return 1;
	} else {
		output = stdout;
	}

	// Citire si procesare fisier de input
	readFile(input, output, hashmap, queue, input_file_name);

	// Inchiderea fisierelor
	if (input_file_name != NULL) {
		free(input_file_name);
		fclose(input);
	}
	if (output_file_name != NULL) {
		free(output_file_name);
		fclose(output);
	}

	// Eliberarea memoriei
	free_hashmap(hashmap);
	destroyQueue(queue);
	return 0;
}
