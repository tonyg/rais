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

static hashtable_t all_vhosts;

void init_vhost(void) {
  init_hashtable(&all_vhosts, 53, NULL, NULL);
}

void done_vhost(void) {
  destroy_hashtable(&all_vhosts);
}

static void create_standard_resources(vhost_t *vhost) {
  int status = 0;
  declare_exchange(&status, vhost, amqp_cstring_bytes(""),
		   lookup_exchange_type(amqp_cstring_bytes("direct")),
		   1, 0, AMQP_EMPTY_TABLE);
  if (status) die("Could not create default exchange: %d", status);
  declare_exchange(&status, vhost, amqp_cstring_bytes("amq.direct"),
		   lookup_exchange_type(amqp_cstring_bytes("direct")),
		   1, 0, AMQP_EMPTY_TABLE);
  if (status) die("Could not create amq.direct exchange: %d", status);
  declare_exchange(&status, vhost, amqp_cstring_bytes("amq.fanout"),
		   lookup_exchange_type(amqp_cstring_bytes("fanout")),
		   1, 0, AMQP_EMPTY_TABLE);
  if (status) die("Could not create amq.fanout exchange: %d", status);
  declare_exchange(&status, vhost, amqp_cstring_bytes("amq.topic"),
		   lookup_exchange_type(amqp_cstring_bytes("topic")),
		   1, 0, AMQP_EMPTY_TABLE);
  if (status) die("Could not create amq.topic exchange: %d", status);
}

vhost_t *declare_vhost(amqp_bytes_t name) {
  vhost_t *v = malloc(sizeof(vhost_t));
  v->name = amqp_bytes_malloc_dup(name);
  init_hashtable(&v->exchanges, 10243, NULL, NULL);
  init_hashtable(&v->queues, 10243, NULL, NULL);
  hashtable_put(&all_vhosts, v->name, v);
  create_standard_resources(v);
  return v;
}

vhost_t *lookup_vhost(amqp_bytes_t name) {
  vhost_t *v = NULL;
  hashtable_get(&all_vhosts, name, (void **) &v);
  return v;
}
