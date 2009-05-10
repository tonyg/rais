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
#include "connection.h"
#include "channel.h"

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
    /*
    case AMQP_QUEUE_DECLARE_METHOD: {
      amqp_queue_declare_t *m = (amqp_queue_declare_t *) frame->payload.method.decoded;
      SEND_METHOD(conn, frame->channel, AMQP_QUEUE_DECLARE_OK_METHOD, amqp_queue_declare_ok_t,
		  m->queue, 0, 0);
      break;
    }
    */

    default:
      close_channel(conn, frame->channel, AMQP_NOT_IMPLEMENTED, "Method %s not implemented",
		    amqp_method_name(frame->payload.method.id));
      break;
  }
}
