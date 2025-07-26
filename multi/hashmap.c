#include "hashmap.h"


int hash(char *word, int size)
{
	int i;
	int h = 0;

	for (i = 0; i < strlen(word); i++)
		h = ((h * 31) % size + word[i] * 19) % size;
	return h % size;
}

struct HashMap *init_hashmap(int size)
{
	struct HashMap *hmap = malloc(sizeof(struct HashMap));

	if (hmap == NULL)
		exit(12);
	hmap->size = size;
	hmap->map = calloc(size, sizeof(struct MapEntry *));
	if (hmap->map == NULL)
		exit(12);
	return hmap;
}

struct MapEntry *createEntry(char *key, char *value)
{
	MapEntry *entry = malloc(sizeof(struct MapEntry));

	if (entry == NULL)
		exit(12);
	entry->key = malloc((strlen(key) + 1) * sizeof(char));
	if (entry->key == NULL)
		exit(12);
	entry->value = malloc((strlen(value) + 1)*sizeof(char));
	if (entry->value == NULL)
		exit(12);
	strcpy(entry->key, key);
	strcpy(entry->value, value);
	entry->next = NULL;
	return entry;
}

int containsKey(struct HashMap *hashmap, char *key)
{
	int hashcode = hash(key, hashmap->size);
	MapEntry *entry = hashmap->map[hashcode];

	while (entry) {
		if (strcmp(entry->key, key) == 0)
			return 1;
		entry = entry->next;
	}
	return 0;
}

void put_entry(struct HashMap *hashmap, char *key, char *value)
{
	int hashcode = hash(key, hashmap->size);
	MapEntry *entry;

	if (!containsKey(hashmap, key)) {
		entry = createEntry(key, value);

		if (hashmap->map[hashcode] == NULL) {
			hashmap->map[hashcode] = entry;
		} else {
			entry->next = hashmap->map[hashcode];
			hashmap->map[hashcode] = entry;
		}
	}
}

char *getV(HashMap *hashmap, char *key)
{
	int hashcode = hash(key, hashmap->size);
	MapEntry *entry = hashmap->map[hashcode];

	while (entry) {
		if (strcmp(entry->key, key) == 0)
			return entry->value;
		entry = entry->next;
	}
	return NULL;
}

void remove_entry(HashMap *hashmap, char *key)
{
	int hashcode = hash(key, hashmap->size);
	MapEntry *entry = hashmap->map[hashcode];
	MapEntry *temp;

	if (strcmp(entry->key, key) == 0) {
		free(hashmap->map[hashcode]->key);
		free(hashmap->map[hashcode]->value);
		free(hashmap->map[hashcode]);
		hashmap->map[hashcode] = NULL;
	} else {
		while (strcmp(entry->next->key, key) != 0)
			entry = entry->next;
		temp = entry->next;
		entry->next = entry->next->next;
		free(temp->key);
		free(temp->value);
		free(temp);
	}
}

void hashmap_to_string(HashMap *hashmap)
{
	int i;
	MapEntry *entry;

	for (i = 0; i < hashmap->size; i++) {
		entry = hashmap->map[i];

		if (entry != NULL) {
			printf("%d: ", i);
			while (entry) {
				printf("%s=%s ", entry->key, entry->value);
				entry = entry->next;
			}
			printf("\n");
		}
	}
}

void free_hashmap(HashMap *hashmap)
{
	int i;
	MapEntry *entry;
	MapEntry *temp;

	for (i = 0; i < hashmap->size; i++) {
		entry = hashmap->map[i];
		while (entry) {
			temp = entry;
			entry = entry->next;
			free(temp->key);
			free(temp->value);
			free(temp);
		}
	}
	free(hashmap->map);
	free(hashmap);
}
