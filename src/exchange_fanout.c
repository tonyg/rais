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

static exchange_type_t fanout;

static void fanout_init(exchange_t *x) {
}

static void fanout_destroy(exchange_t *x) {
}

void init_exchange_fanout(void) {
  fanout.name = amqp_cstring_bytes("fanout");
  fanout.init = fanout_init;
  fanout.destroy = fanout_destroy;
  register_exchange_type(&fanout);
}
