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

static exchange_type_t direct;

static void direct_init(exchange_t *x) {
}

static void direct_destroy(exchange_t *x) {
}

static void direct_bind(int *status,
			exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments)
{
  hashtable_t *tab = NULL;
  if (!hashtable_get(&x->direct, rk, (void **) &tab)) {
    tab = malloc(sizeof(hashtable_t));
    init_hashtable(tab, 127, NULL, NULL);
    hashtable_put(&x->direct, rk, tab);
  }
  hashtable_put(tab, q->name, q);
}

static void direct_unbind(int *status,
			  exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments)
{
  hashtable_t *tab = NULL;
  if (!hashtable_get(&x->direct, rk, (void **) &tab)) {
    return;
  }
  hashtable_erase(tab, q->name);
  if (tab->entry_count == 0) {
    hashtable_erase(&x->direct, rk);
    destroy_hashtable(tab);
    free(tab);
  }
}

void init_exchange_direct(void) {
  direct.name = amqp_cstring_bytes("direct");
  direct.init = direct_init;
  direct.destroy = direct_destroy;
  direct.bind = direct_bind;
  direct.unbind = direct_unbind;
  register_exchange_type(&direct);
}
