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

static exchange_type_t topic;

static void topic_init(exchange_t *x) {
}

static void topic_destroy(exchange_t *x) {
}

void init_exchange_topic(void) {
  topic.name = amqp_cstring_bytes("topic");
  topic.init = topic_init;
  topic.destroy = topic_destroy;
  register_exchange_type(&topic);
}
