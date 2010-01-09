#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <stdint.h>
#include <amqp.h>
#include <amqp_framing.h>

#include <netdb.h>

#include <assert.h>

#include "config.h"
#include "util.h"
#include "rais.h"
#include "queue.h"
#include "hashtable.h"

static hashtable_t all_queues;

void init_queue(void) {
  init_hashtable(&all_queues, 10243, NULL, NULL);
}

void done_queue(void) {
  destroy_hashtable(&all_queues);
}

static queue_t *internal_lookup_queue(amqp_bytes_t flat_name) {
  queue_t *result = NULL;
  hashtable_get(&all_queues, flat_name, (void **) &result);
  return result;
}

queue_t *declare_queue(int *status,
		       resource_name_t const *name,
		       amqp_boolean_t durable,
		       amqp_boolean_t auto_delete,
		       amqp_table_t arguments)
{
  amqp_bytes_t flat_name = flatten_name(name);
  queue_t *q = internal_lookup_queue(flat_name);

  if (q == NULL) {
    q = malloc(sizeof(queue_t));
    q->name.vhost = amqp_bytes_malloc_dup(name->vhost);
    if (name->name.len == 0) {
      q->name.name = amqp_bytes_malloc(20);
      gensym(q->name.name.bytes, q->name.name.len, "amq.q.");
      q->name.name.len = strlen(q->name.name.bytes);
    } else {
      q->name.name = amqp_bytes_malloc_dup(name->name);
    }
    q->durable = durable;
    q->auto_delete = auto_delete;
    q->arguments = AMQP_EMPTY_TABLE; /* TODO: copy arguments */
    q->queue_len = 0;
    q->consumer_count = 0;
    info("Queue %*s created", flat_name.len, flat_name.bytes);
    hashtable_put(&all_queues, flat_name, q);
  }

  AMQP_BYTES_FREE(flat_name);
  return q;
}

queue_t *lookup_queue(int *status,
		      resource_name_t const *name)
{
  amqp_bytes_t flat_name = flatten_name(name);
  queue_t *q = internal_lookup_queue(flat_name);

  if (q == NULL) {
    *status = AMQP_NOT_FOUND;
  }

  AMQP_BYTES_FREE(flat_name);
  return q;
}
