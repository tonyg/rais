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
#include "hashtable.h"
#include "syncpipe.h"
#include "vhost.h"
#include "queue.h"
#include "exchange.h"

void init_queue(void) {
}

void done_queue(void) {
}

static queue_t *internal_lookup_queue(vhost_t *vhost, amqp_bytes_t name) {
  queue_t *result = NULL;
  hashtable_get(&vhost->queues, name, (void **) &result);
  return result;
}

static void bind_to_default_exchange(vhost_t *vhost, queue_t *q) {
  int status = 0;
  exchange_t *x = lookup_exchange(&status, vhost, AMQP_EMPTY_BYTES);
  if (status) {
    /* Missing default exchange. Continue. */
    warn("Default exchange not found (%d); continuing without binding queue \"%.*s\"",
	 status, q->name.len, q->name.bytes);
    return;
  }
  exchange_bind(&status, x, q, q->name, AMQP_EMPTY_TABLE);
  if (status) {
    warn("Default exchange binding failed %s(%d)",
	 amqp_constant_name(status), status);
  }
}

queue_t *declare_queue(int *status,
		       vhost_t *vhost,
		       amqp_bytes_t name,
		       amqp_boolean_t durable,
		       amqp_boolean_t auto_delete,
		       amqp_table_t arguments)
{
  queue_t *q = internal_lookup_queue(vhost, name);

  if (q == NULL) {
    q = malloc(sizeof(queue_t));
    if (name.len == 0) {
      q->name = amqp_bytes_malloc(20);
      gensym(q->name.bytes, q->name.len, "amq.q.");
      q->name.len = strlen(q->name.bytes);
    } else {
      q->name = amqp_bytes_malloc_dup(name);
    }
    q->durable = durable;
    q->auto_delete = auto_delete;
    q->arguments = AMQP_EMPTY_TABLE; /* TODO: copy arguments */
    q->queue_len = 0;
    q->consumer_count = 0;
    info("Queue \"%.*s\" created", name.len, name.bytes);
    hashtable_put(&vhost->queues, name, q);
    bind_to_default_exchange(vhost, q);
  }

  return q;
}

queue_t *lookup_queue(int *status,
		      vhost_t *vhost,
		      amqp_bytes_t name)
{
  queue_t *q = internal_lookup_queue(vhost, name);

  if (q == NULL) {
    *status = AMQP_NOT_FOUND;
  }

  return q;
}
