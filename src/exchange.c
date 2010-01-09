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
#include "vhost.h"
#include "exchange.h"

static hashtable_t all_exchange_types;

extern void init_exchange_direct(void);
extern void init_exchange_fanout(void);
extern void init_exchange_topic(void);

void init_exchange(void) {
  init_hashtable(&all_exchange_types, 53, NULL, NULL);
  init_exchange_direct();
  init_exchange_fanout();
  init_exchange_topic();
}

void done_exchange(void) {
  destroy_hashtable(&all_exchange_types);
}

void register_exchange_type(exchange_type_t *type) {
  hashtable_put(&all_exchange_types, type->name, type);
}

exchange_type_t *lookup_exchange_type(amqp_bytes_t name) {
  exchange_type_t *t = NULL;
  hashtable_get(&all_exchange_types, name, (void **) &t);
  return t;
}

static exchange_t *internal_lookup_exchange(vhost_t *vhost, amqp_bytes_t name) {
  exchange_t *result = NULL;
  hashtable_get(&vhost->exchanges, name, (void **) &result);
  return result;
}

exchange_t *declare_exchange(int *status,
			     vhost_t *vhost,
			     amqp_bytes_t name,
			     exchange_type_t *type,
			     amqp_boolean_t durable,
			     amqp_boolean_t auto_delete,
			     amqp_table_t arguments)
{
  exchange_t *x = internal_lookup_exchange(vhost, name);

  if (x == NULL) {
    x = malloc(sizeof(exchange_t));
    x->name = amqp_bytes_malloc_dup(name);
    x->type = type;
    x->type_data = NULL;
    x->durable = durable;
    x->auto_delete = auto_delete;
    x->arguments = AMQP_EMPTY_TABLE; /* TODO: copy arguments */
    type->init(x);
    info("Exchange \"%.*s\" of type %.*s created",
	 name.len, name.bytes,
	 type->name.len, type->name.bytes);
    hashtable_put(&vhost->exchanges, name, x);
  }

  return x;
}

exchange_t *lookup_exchange(int *status,
			    vhost_t *vhost,
			    amqp_bytes_t name)
{
  exchange_t *x = internal_lookup_exchange(vhost, name);

  if (x == NULL) {
    *status = AMQP_NOT_FOUND;
  }

  return x;
}
