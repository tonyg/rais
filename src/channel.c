#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <stdint.h>
#include <amqp.h>
#include <amqp_framing.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>

#include <stdarg.h>
#include <assert.h>

#include <event.h>

#include "config.h"
#include "util.h"
#include "hashtable.h"
#include "vhost.h"
#include "connection.h"
#include "channel.h"
#include "exchange.h"
#include "queue.h"

chanstate_t *new_channel_state(amqp_channel_t channel) {
  chanstate_t *chan = calloc(1, sizeof(chanstate_t));
  chan->channel = channel;
  return chan;
}

void destroy_channel_state(chanstate_t *chan) {
  free(chan);
}

void handle_channel_normal(connstate_t *conn,
			   amqp_frame_t *frame,
			   chanstate_t *chan)
{
  ENSURE_FRAME_IS_METHOD(conn, frame);

  switch (frame->payload.method.id) {
    case AMQP_EXCHANGE_DECLARE_METHOD: {
      amqp_exchange_declare_t *m = (amqp_exchange_declare_t *) frame->payload.method.decoded;
      exchange_type_t *type;
      exchange_t *x;

      type = lookup_exchange_type(m->type);
      if (type == NULL) {
	chan->status = AMQP_COMMAND_INVALID;
	break;
      }

      x = m->passive
	? lookup_exchange(&chan->status, conn->vhost, m->exchange)
	: declare_exchange(&chan->status, conn->vhost, m->exchange,
			   type, m->durable, m->auto_delete, m->arguments);
      if (!chan->status) {
	SEND_METHOD(conn, chan->channel,
		    AMQP_EXCHANGE_DECLARE_OK_METHOD, amqp_exchange_declare_ok_t);
      }
      break;
    }

    case AMQP_QUEUE_DECLARE_METHOD: {
      amqp_queue_declare_t *m = (amqp_queue_declare_t *) frame->payload.method.decoded;
      queue_t *q;

      q = m->passive
	? lookup_queue(&chan->status, conn->vhost, m->queue)
	: declare_queue(&chan->status, conn->vhost, m->queue,
			m->durable, m->auto_delete, m->arguments);
      if (!chan->status) {
	SEND_METHOD(conn, chan->channel, AMQP_QUEUE_DECLARE_OK_METHOD, amqp_queue_declare_ok_t,
		    q->name, q->queue_len, q->consumer_count);
      }
      break;
    }

    case AMQP_BASIC_PUBLISH_METHOD: {
      amqp_basic_publish_t *m = (amqp_basic_publish_t *) frame->payload.method.decoded;
      exchange_t *x;

      if (m->mandatory || m->immediate) {
	chan->status = AMQP_NOT_IMPLEMENTED;
	break;
      }

      x = lookup_exchange(&chan->status, conn->vhost, m->exchange);
      if (chan->status) break;

      chan->status = AMQP_INTERNAL_ERROR; /* HERE */
      break;
    }

    default:
      chan->status = AMQP_NOT_IMPLEMENTED;
      break;
  }

  if (chan->status) {
    if (amqp_constant_is_hard_error(chan->status)) {
      close_connection(conn, chan->status, "Connection error %s (%d) in response to %s",
		       amqp_constant_name(chan->status),
		       chan->status,
		       amqp_method_name(frame->payload.method.id));
    } else {
      close_channel(conn, chan->channel, chan->status, "Channel error %s (%d) in response to %s",
		    amqp_constant_name(chan->status),
		    chan->status,
		    amqp_method_name(frame->payload.method.id));
    }
  }
}
