#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <assert.h>

#include <amqp.h>

#include "hashtable.h"

uint32_t hash_bytes(amqp_bytes_t bytes) {
  /* http://en.wikipedia.org/wiki/Jenkins_hash_function */
  uint32_t hash = 0;
  size_t i;
 
  for (i = 0; i < bytes.len; i++) {
    hash += ((unsigned char *) bytes.bytes)[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash;
}

void init_hashtable(hashtable_t *table,
		    size_t initial_bucket_count,
		    void *(*dup_value)(void *),
		    void (*free_value)(void *))
{
  table->bucket_count = initial_bucket_count;
  table->entry_count = 0;
  table->buckets = NULL;
  table->dup_value = dup_value;
  table->free_value = free_value;

  if (initial_bucket_count > 0) {
    table->buckets = realloc(table->buckets,
			     initial_bucket_count * sizeof(hashtable_entry_t *));
  }
}

static void destroy_entry(hashtable_t *table, hashtable_entry_t *entry) {
  AMQP_BYTES_FREE(entry->key);
  if (table->free_value != NULL) {
    table->free_value(entry->value);
  }
  free(entry);
}

void destroy_hashtable(hashtable_t *table) {
  if (table->buckets != NULL) {
    int i;
    for (i = 0; i < table->bucket_count; i++) {
      hashtable_entry_t *chain = table->buckets[i];
      table->buckets[i] = NULL;
      while (chain != NULL) {
	hashtable_entry_t *next = chain->next;
	destroy_entry(table, chain);
	chain = next;
      }
    }
    free(table->buckets);
  }
}

static hashtable_entry_t **hashtable_find(hashtable_t *table, amqp_bytes_t key) {
  uint32_t h = hash_bytes(key) % table->bucket_count;
  hashtable_entry_t **entryptr = &(table->buckets[h]);
  hashtable_entry_t *entry = *entryptr;
  while (entry != NULL) {
    if ((entry->key.len == key.len) && !memcmp(entry->key.bytes, key.bytes, key.len)) {
      break;
    }
    entryptr = &entry->next;
    entry = *entryptr;
  }
  return entryptr;
}

int hashtable_get(hashtable_t *table, amqp_bytes_t key, void **valueptr) {
  hashtable_entry_t **entryptr = hashtable_find(table, key);
  if (*entryptr == NULL) {
    return 0;
  } else {
    *valueptr = (*entryptr)->value;
    return 1;
  }
}

int hashtable_put(hashtable_t *table, amqp_bytes_t key, void *value) {
  /* TODO: grow and rehash */
  hashtable_entry_t **entryptr = hashtable_find(table, key);
  if (*entryptr == NULL) {
    hashtable_entry_t *entry = malloc(sizeof(hashtable_entry_t));
    entry->next = NULL;
    entry->key = amqp_bytes_malloc_dup(key);
    entry->value = (table->dup_value == NULL) ? value : table->dup_value(value);
    *entryptr = entry;
    table->entry_count++;
    return 1;
  } else {
    if (table->free_value != NULL) {
      table->free_value((*entryptr)->value);
    }
    (*entryptr)->value = (table->dup_value == NULL) ? value : table->dup_value(value);
    return 0;
  }
}

int hashtable_erase(hashtable_t *table, amqp_bytes_t key) {
  hashtable_entry_t **entryptr = hashtable_find(table, key);
  if (*entryptr == NULL) {
    return 0;
  } else {
    hashtable_entry_t *entry = *entryptr;
    *entryptr = entry->next;
    destroy_entry(table, entry);
    table->entry_count--;
    return 1;
  }
}

void hashtable_foreach(hashtable_t *table,
		       hashtable_iterator_t iterator,
		       void *context)
{
  int i;
  for (i = 0; i < table->bucket_count; i++) {
    hashtable_entry_t *chain;
    for (chain = table->buckets[i]; chain != NULL; chain = chain->next) {
      iterator(context, chain->key, chain->value);
    }
  }
}
