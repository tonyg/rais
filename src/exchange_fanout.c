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

static exchange_type_t fanout;

static void fanout_init(exchange_t *x) {
}

static void fanout_destroy(exchange_t *x) {
}

static void fanout_bind(int *status,
			exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments)
{
  hashtable_put(&x->fanout, q->name, q);
}

static void fanout_unbind(int *status,
			  exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments)
{
  hashtable_erase(&x->fanout, q->name);
}

void init_exchange_fanout(void) {
  fanout.name = amqp_cstring_bytes("fanout");
  fanout.init = fanout_init;
  fanout.destroy = fanout_destroy;
  fanout.bind = fanout_bind;
  fanout.unbind = fanout_unbind;
  register_exchange_type(&fanout);
}
