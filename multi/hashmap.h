#include<stdio.h>
#include<stdlib.h>
#include<string.h>

typedef struct MapEntry {
	char *key;
	char *value;
	struct MapEntry *next;
} MapEntry;


typedef struct HashMap {
	int size;
	struct MapEntry **map;
} HashMap;

int hash(char *word, int size);

struct HashMap *init_hashmap(int size);

struct MapEntry *createEntry(char *key, char *value);

int containsKey(struct HashMap *hashmap, char *key);

void put_entry(struct HashMap *hashmap, char *key, char *value);

char *getV(HashMap *hashmap, char *key);

void remove_entry(HashMap *hashmap, char *key);

void hashmap_to_string(HashMap *hashmap);

void free_hashmap(HashMap *hashmap);
