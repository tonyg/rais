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
#include "syncpipe.h"
#include "vhost.h"
#include "connection.h"
#include "channel.h"
#include "queue.h"
#include "exchange.h"

chanstate_t *new_channel_state(amqp_channel_t channel) {
  chanstate_t *chan = calloc(1, sizeof(chanstate_t));
  chan->channel = channel;
  chan->status = 0;
  chan->next_exchange = AMQP_EMPTY_BYTES;
  chan->next_routing_key = AMQP_EMPTY_BYTES;
  chan->body_remaining = 0;
  chan->content_pipe = NULL_SYNCPIPE_OUT;
  return chan;
}

void destroy_channel_state(chanstate_t *chan) {
  syncpipe_close_out(chan->content_pipe);
  AMQP_BYTES_FREE(chan->next_routing_key);
  AMQP_BYTES_FREE(chan->next_exchange);
  free(chan);
}

static amqp_boolean_t check_channel_status(connstate_t *conn,
					   amqp_frame_t *frame,
					   chanstate_t *chan) {
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
    return 1;
  } else {
    return 0;
  }
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

      if (m->mandatory || m->immediate) {
	chan->status = AMQP_NOT_IMPLEMENTED;
	break;
      }

      chan->next_exchange = amqp_bytes_malloc_dup(m->exchange);
      chan->next_routing_key = amqp_bytes_malloc_dup(m->routing_key);
      /* will transition to content-accepting state; next is handle_channel_props */
      break;
    }

    default:
      chan->status = AMQP_NOT_IMPLEMENTED;
      break;
  }

  check_channel_status(conn, frame, chan);
}

static void check_body_completion(connstate_t *conn, chanstate_t *chan)
{
  if (chan->body_remaining == 0) {
    syncpipe_close_out(chan->content_pipe);
    chan->content_pipe = NULL_SYNCPIPE_OUT;
    set_channel_callback(conn, chan->channel, &handle_channel_normal);
  }
}

void handle_channel_props(connstate_t *conn,
			  amqp_frame_t *frame,
			  chanstate_t *chan)
{
  exchange_t *x;

  if (frame->frame_type != AMQP_FRAME_HEADER) {
    close_connection_command_invalid(conn, frame);
    return;
  }

  if (frame->payload.properties.class_id != AMQP_BASIC_CLASS) {
    close_connection_command_invalid(conn, frame);
    return;
  }

  x = lookup_exchange(&chan->status, conn->vhost, chan->next_exchange);
  if (check_channel_status(conn, frame, chan)) return;

  chan->body_remaining = frame->payload.properties.body_size;
  chan->content_pipe =
    exchange_route(&chan->status,
		   x,
		   chan->next_routing_key,
		   (amqp_basic_properties_t *) frame->payload.properties.decoded,
		   chan->body_remaining,
		   frame->payload.properties.raw);
  if (check_channel_status(conn, frame, chan)) return;

  AMQP_BYTES_FREE(chan->next_routing_key);
  AMQP_BYTES_FREE(chan->next_exchange);

  set_channel_callback(conn, chan->channel, &handle_channel_body);
  check_body_completion(conn, chan);
}

static void mark_buffer_consumed(amqp_boolean_t *marker, amqp_bytes_t data) {
  *marker = 1;
}

void handle_channel_body(connstate_t *conn,
			 amqp_frame_t *frame,
			 chanstate_t *chan)
{
  amqp_boolean_t buffer_was_consumed = 0;

  if (frame->frame_type != AMQP_FRAME_BODY) {
    close_connection_command_invalid(conn, frame);
    return;
  }

  syncpipe_write(chan->content_pipe,
		 frame->payload.body_fragment,
		 &buffer_was_consumed,
		 (syncpipe_callback_t) &mark_buffer_consumed);
  if (!buffer_was_consumed) {
    die("Transient body buffer was not consumed");
  }

  check_body_completion(conn, chan);
}
