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

static exchange_type_t topic;

static void topic_init(exchange_t *x) {
}

static void topic_destroy(exchange_t *x) {
}

static void topic_bind(int *status,
		       exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments)
{
  *status = AMQP_NOT_IMPLEMENTED;
}

static void topic_unbind(int *status,
			 exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments)
{
  *status = AMQP_NOT_IMPLEMENTED;
}

void init_exchange_topic(void) {
  topic.name = amqp_cstring_bytes("topic");
  topic.init = topic_init;
  topic.destroy = topic_destroy;
  topic.bind = topic_bind;
  topic.unbind = topic_unbind;
  register_exchange_type(&topic);
}
