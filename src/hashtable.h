#ifndef rais_hashtable_h
#define rais_hashtable_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hashtable_entry_t_ {
  struct hashtable_entry_t_ *next;
  amqp_bytes_t key;
  void *value;
} hashtable_entry_t;

typedef struct hashtable_t_ {
  size_t bucket_count;
  size_t entry_count;
  hashtable_entry_t **buckets;
  void *(*dup_value)(void *);
  void (*free_value)(void *);
} hashtable_t;

typedef void (*hashtable_iterator_t)(void *context, amqp_bytes_t key, void *value);

extern uint32_t hash_bytes(amqp_bytes_t bytes);

extern void init_hashtable(hashtable_t *table,
			   size_t initial_bucket_count,
			   void *(*dup_value)(void *),
			   void (*free_value)(void *));
extern void destroy_hashtable(hashtable_t *table);

extern int hashtable_get(hashtable_t *table, amqp_bytes_t key, void **valueptr);
extern int hashtable_put(hashtable_t *table, amqp_bytes_t key, void *value);
extern int hashtable_erase(hashtable_t *table, amqp_bytes_t key);
extern void hashtable_foreach(hashtable_t *table,
			      hashtable_iterator_t iterator,
			      void *context);

#ifdef __cplusplus
}
#endif

#endif
