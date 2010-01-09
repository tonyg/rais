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

static exchange_type_t direct;

static void direct_init(exchange_t *x) {
}

static void direct_destroy(exchange_t *x) {
}

void init_exchange_direct(void) {
  direct.name = amqp_cstring_bytes("direct");
  direct.init = direct_init;
  direct.destroy = direct_destroy;
  register_exchange_type(&direct);
}
