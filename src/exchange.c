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
#include "exchange.h"
#include "hashtable.h"

static hashtable_t all_exchange_types;
static hashtable_t all_exchanges;

void init_exchange(void) {
  init_hashtable(&all_exchange_types, 53, NULL, NULL);
  init_hashtable(&all_exchanges, 10243, NULL, NULL);
}

void done_exchange(void) {
  destroy_hashtable(&all_exchanges);
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

static exchange_t *internal_lookup_exchange(amqp_bytes_t flat_name) {
  exchange_t *result = NULL;
  hashtable_get(&all_exchanges, flat_name, (void **) &result);
  return result;
}

exchange_t *declare_exchange(int *status,
			     resource_name_t const *name,
			     exchange_type_t *type,
			     amqp_boolean_t durable,
			     amqp_boolean_t auto_delete,
			     amqp_table_t arguments)
{
  amqp_bytes_t flat_name = flatten_name(name);
  exchange_t *x = internal_lookup_exchange(flat_name);

  if (x == NULL) {
    x = malloc(sizeof(exchange_t));
    x->name.vhost = amqp_bytes_malloc_dup(name->vhost);
    x->name.name = amqp_bytes_malloc_dup(name->name);
    x->type = type;
    x->type_data = NULL;
    x->durable = durable;
    x->auto_delete = auto_delete;
    x->arguments = AMQP_EMPTY_TABLE; /* TODO: copy arguments */
    type->init(x);
    info("Exchange %*s of type %*s created",
	 flat_name.len, flat_name.bytes,
	 type->name.len, type->name.bytes);
    hashtable_put(&all_exchanges, flat_name, x);
  }

  AMQP_BYTES_FREE(flat_name);
  return x;
}

exchange_t *lookup_exchange(int *status,
			    resource_name_t const *name)
{
  amqp_bytes_t flat_name = flatten_name(name);
  exchange_t *x = internal_lookup_exchange(flat_name);

  if (x == NULL) {
    *status = AMQP_NOT_FOUND;
  }

  AMQP_BYTES_FREE(flat_name);
  return x;
}
