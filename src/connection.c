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

static char const *connection_name(connstate_t *conn) {
  char name[256];
  static char result[256];
  get_addr_name(name, sizeof(name), &conn->peername);
  snprintf(result, sizeof(result), "%s:%d", name, ntohs(conn->peername.sin_port));
  return result;
}

static void destroy_connection(connstate_t *conn) {
  info("Shutting down connection from %s on fd %d", connection_name(conn), conn->fd);

  AMQP_BYTES_FREE(conn->vhost);
  bufferevent_disable(conn->io, EV_READ | EV_WRITE);
  bufferevent_free(conn->io);
  amqp_destroy_connection(conn->amqp_conn);
  close(conn->fd);
  free(conn);
}

void send_method(connstate_t *conn,
		 amqp_channel_t channel,
		 amqp_method_number_t id,
		 void *decoded)
{
  amqp_frame_t frame;

  frame.frame_type = AMQP_FRAME_METHOD;
  frame.channel = channel;
  frame.payload.method.id = id;
  frame.payload.method.decoded = decoded;
  amqp_send_frame_to(conn->amqp_conn,
		     &frame,
		     (amqp_output_fn_t) bufferevent_write,
		     conn->io);
}

void close_connection(connstate_t *conn, short code, char const *explanation, ...) {
  char buf[4096];
  va_list vl;
  char *preamble;
  int i;

  va_start(vl, explanation);
  vsnprintf(buf, sizeof(buf), explanation, vl);
  va_end(vl);

  if (conn->state == CONNECTION_STATE_READY) {
    preamble = "Sending connection.close";
  } else {
    preamble = "Closing connection";
  }

  if (code == AMQP_REPLY_SUCCESS) {
    info("%s with code %d: %s", preamble, code, buf);
  } else {
    warn("%s with code %d: %s", preamble, code, buf);
  }

  assert(conn->channel_state[0] == NULL);
  for (i = 1; i <= MAX_CHANNELS; i++) {
    if (conn->channel_state[i] != NULL) {
      destroy_channel_state(conn->channel_state[i]);
      conn->channel_state[i] = NULL;
    }
  }

  if (conn->state == CONNECTION_STATE_READY) {
    SEND_METHOD(conn, 0, AMQP_CONNECTION_CLOSE_METHOD, amqp_connection_close_t,
		code, amqp_cstring_bytes(buf), 0, 0);
  } else {
    destroy_connection(conn);
  }
}

static void set_channel_callback(connstate_t *conn,
				 amqp_channel_t ch,
				 channel_callback_t cb)
{
  assert(ch <= MAX_CHANNELS);
  conn->channel_callback[ch] = cb;
}

/* forward: */
static void handle_channel_when_closed(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan);

static void channel_closewait(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan)
{
  if (frame->frame_type != AMQP_FRAME_METHOD) {
    return;
  }

  if (frame->payload.method.id != AMQP_CHANNEL_CLOSE_OK_METHOD) {
    return;
  }

  destroy_channel_state(conn->channel_state[frame->channel]);
  conn->channel_state[frame->channel] = NULL;
  set_channel_callback(conn, frame->channel, &handle_channel_when_closed);
}

void close_channel(connstate_t *conn,
		   amqp_channel_t ch,
		   short code,
		   char const *explanation,
		   ...)
{
  char buf[4096];
  va_list vl;
  char *preamble;

  assert(ch <= MAX_CHANNELS);

  va_start(vl, explanation);
  vsnprintf(buf, sizeof(buf), explanation, vl);
  va_end(vl);

  if (conn->channel_callback[ch] != &channel_closewait) {
    preamble = "Sending channel.close";
  } else {
    preamble = "Closing channel";
  }

  if (code == AMQP_REPLY_SUCCESS) {
    info("%s %d with code %d: %s", preamble, ch, code, buf);
  } else {
    warn("%s %d with code %d: %s", preamble, ch, code, buf);
  }

  if (conn->channel_callback[ch] != &channel_closewait) {
    set_channel_callback(conn, ch, &channel_closewait);
    SEND_METHOD(conn, ch, AMQP_CHANNEL_CLOSE_METHOD, amqp_channel_close_t,
		code, amqp_cstring_bytes(buf), 0, 0);
  }
}

void close_connection_command_invalid(connstate_t *conn, amqp_frame_t *frame) {
  close_connection(conn, AMQP_COMMAND_INVALID, "Unexpected frame %d/%08X",
		   frame->frame_type,
		   frame->payload.method.id);
}

#define CHECK_INBOUND_METHOD(conn, expectedid, receivedframe)	\
  ({								\
    ENSURE_FRAME_IS_METHOD(conn, receivedframe);		\
    if (receivedframe->payload.method.id != expectedid) {	\
      close_connection_command_invalid(conn, receivedframe);	\
      return;							\
    }								\
    (void *) frame->payload.method.decoded;			\
  })

static void handle_connection_misc(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan)
{
  ENSURE_FRAME_IS_METHOD(conn, frame);
  switch (frame->payload.method.id) {
    case AMQP_CONNECTION_CLOSE_METHOD:
      die("Argh connection close!");
      break;

    case AMQP_CONNECTION_CLOSE_OK_METHOD:
      conn->state = CONNECTION_STATE_DEAD;
      close_connection(conn, 200, "connection.close-ok received");
      return;
    
    default:
      close_connection_command_invalid(conn, frame);
      return;
  }
}

static void handle_channel_when_closed(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan)
{
  ENSURE_FRAME_IS_METHOD(conn, frame);
  switch (frame->payload.method.id) {
    case AMQP_CHANNEL_OPEN_METHOD:
      conn->channel_state[frame->channel] = new_channel_state(frame->channel);
      set_channel_callback(conn, frame->channel, &handle_channel_normal);
      SEND_METHOD(conn, frame->channel, AMQP_CHANNEL_OPEN_OK_METHOD, amqp_channel_open_ok_t);
      break;

    default:
      close_connection(conn, AMQP_CHANNEL_ERROR, "Operation on closed channel");
      break;
  }
}

static void handle_connection_open(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan)
{
  amqp_connection_open_t *m = CHECK_INBOUND_METHOD(conn,
						   AMQP_CONNECTION_OPEN_METHOD,
						   frame);
  int i;

  info("Opening vhost %.*s",
       (int) m->virtual_host.len, (char *) m->virtual_host.bytes);
  conn->vhost = amqp_bytes_malloc_dup(m->virtual_host);

  conn->state = CONNECTION_STATE_READY;
  set_channel_callback(conn, 0, &handle_connection_misc);
  bufferevent_settimeout(conn->io, 0, 0); /* TODO revisit heartbeats */

  for (i = 1; i <= MAX_CHANNELS; i++) {
    set_channel_callback(conn, i, &handle_channel_when_closed);
  }

  SEND_METHOD(conn, 0, AMQP_CONNECTION_OPEN_OK_METHOD, amqp_connection_open_ok_t,
	      AMQP_EMPTY_BYTES);
}

static void handle_tune_ok(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan)
{
  amqp_connection_tune_ok_t *m = CHECK_INBOUND_METHOD(conn,
						      AMQP_CONNECTION_TUNE_OK_METHOD,
						      frame);
  amqp_tune_connection(conn->amqp_conn, m->channel_max, m->frame_max);
  set_channel_callback(conn, 0, &handle_connection_open);
}

static void handle_start_ok(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan)
{
  amqp_connection_start_ok_t *m = CHECK_INBOUND_METHOD(conn,
						       AMQP_CONNECTION_START_OK_METHOD,
						       frame);
  char *username = memchr(m->response.bytes, 0, m->response.len);
  char *password;
  size_t username_len, password_len;
  if (username == NULL) {
    close_connection(conn, AMQP_ACCESS_REFUSED, "Access refused");
    return;
  }

  username = username + 1;
  password = memchr(username, 0, m->response.len - (username - (char *) m->response.bytes));
  if (password == NULL) {
    close_connection(conn, AMQP_ACCESS_REFUSED, "Access refused");
    return;
  }
  username_len = password - username;
  password = password + 1;
  password_len = m->response.len - (password - (char *) m->response.bytes);

  info("Login mechanism is %.*s; user %.*s, pass %.*s",
       (int) m->mechanism.len, (char *) m->mechanism.bytes,
       username_len, username, password_len, password);

  set_channel_callback(conn, 0, &handle_tune_ok);
  SEND_METHOD(conn, 0, AMQP_CONNECTION_TUNE_METHOD, amqp_connection_tune_t,
	      MAX_CHANNELS, SUGGESTED_FRAME_MAX, 0);

}

static void handle_protocol_header(connstate_t *conn, amqp_frame_t *frame) {
  if (conn->state != CONNECTION_STATE_INITIAL) {
    close_connection(conn, AMQP_FRAME_ERROR, "Duplicate protocol header");
    return;
  }

  if ((frame->payload.protocol_header.transport_high != 1) ||
      (frame->payload.protocol_header.transport_low != 1) ||
      (frame->payload.protocol_header.protocol_version_major != AMQP_PROTOCOL_VERSION_MAJOR) ||
      (frame->payload.protocol_header.protocol_version_minor != AMQP_PROTOCOL_VERSION_MINOR))
  {
    amqp_send_header_to(conn->amqp_conn, (amqp_output_fn_t) bufferevent_write, conn->io);
    close_connection(conn, AMQP_NOT_IMPLEMENTED, "Protocol version mismatch %d:%d:%d:%d",
		     (int) frame->payload.protocol_header.transport_high,
		     (int) frame->payload.protocol_header.transport_low,
		     (int) frame->payload.protocol_header.protocol_version_major,
		     (int) frame->payload.protocol_header.protocol_version_minor);
    return;
  }

  conn->state = CONNECTION_STATE_HEADER_RECEIVED;

  {
    amqp_table_entry_t entries[2] = { AMQP_TABLE_ENTRY_S("product", amqp_cstring_bytes("rais")),
				      AMQP_TABLE_ENTRY_S("version", amqp_cstring_bytes(VERSION)) };
    amqp_table_t server_info = { .num_entries = sizeof(entries) / sizeof(entries[0]),
				 .entries = &entries[0] };

    set_channel_callback(conn, 0, &handle_start_ok);
    SEND_METHOD(conn, 0, AMQP_CONNECTION_START_METHOD, amqp_connection_start_t,
		AMQP_PROTOCOL_VERSION_MAJOR,
		AMQP_PROTOCOL_VERSION_MINOR,
		server_info,
		amqp_cstring_bytes("PLAIN"),
		amqp_cstring_bytes("en_US"));
  }
}

static void handle_frame(connstate_t *conn, amqp_frame_t *frame) {
  switch (frame->frame_type) {
    case AMQP_PSEUDOFRAME_PROTOCOL_HEADER:
      handle_protocol_header(conn, frame);
      return;

    case AMQP_FRAME_METHOD:
    case AMQP_FRAME_HEADER:
    case AMQP_FRAME_BODY:
      if (frame->channel > MAX_CHANNELS || conn->channel_callback[frame->channel] == NULL) {
	close_connection(conn, AMQP_CHANNEL_ERROR, "Invalid channel %d", frame->channel);
      } else {
	(conn->channel_callback[frame->channel])(conn,
						 frame,
						 conn->channel_state[frame->channel]);
      }
      return;

    case AMQP_FRAME_HEARTBEAT:
      /* Echo it back. TODO revisit heartbeat */
      amqp_send_frame_to(conn->amqp_conn,
			 frame,
			 (amqp_output_fn_t) bufferevent_write,
			 conn->io);
      return;

    default:
      close_connection(conn, AMQP_FRAME_ERROR, "Could not handle frame type %d", frame->frame_type);
      return;
  }
}

static void read_callback(struct bufferevent *bufev, connstate_t *conn) {
  amqp_bytes_t input;
  amqp_frame_t frame;

  while (1) {
    int nread;

    input.bytes = bufev->input->buffer;
    input.len = bufev->input->off;

    if (input.len == 0) {
      break;
    }

    nread = amqp_handle_input(conn->amqp_conn, input, &frame);
    if (nread < 0) {
      conn->state = CONNECTION_STATE_DEAD;
      close_connection(conn, AMQP_FRAME_ERROR, "Could not parse frame");
      return;
    }
    evbuffer_drain(bufev->input, nread);

    if (frame.frame_type != 0) {
      handle_frame(conn, &frame);
      amqp_maybe_release_buffers(conn->amqp_conn);
    }
  }
}

static void error_callback(struct bufferevent *bufev, short what, connstate_t *conn) {
  short direction = what & (EVBUFFER_READ | EVBUFFER_WRITE);
  short kind = what & ~(EVBUFFER_READ | EVBUFFER_WRITE);

  switch (kind) {
    case EVBUFFER_TIMEOUT:
      conn->state = CONNECTION_STATE_DEAD;
      close_connection(conn, 0, "Handshake timeout");
      break;

    case EVBUFFER_EOF:
      conn->state = CONNECTION_STATE_DEAD;
      close_connection(conn, 200, "Connection closed by peer");
      break;

    default:
      /* %%% FIXME */
      die("Unimplemented default branch 0x%04x (dir 0x%04x, kind 0x%04x) in error_callback",
	  what, direction, kind);
      break;
  }
}

void start_inbound_connection(struct sockaddr_in const *peername, int fd) {
  connstate_t *conn = calloc(1, sizeof(connstate_t));
  conn->peername = *peername;
  conn->fd = fd;
  conn->amqp_conn = amqp_new_connection();
  amqp_set_sockfd(conn->amqp_conn, fd);
  conn->io = bufferevent_new(fd,
				   (evbuffercb) read_callback,
				   NULL,
				   (everrorcb) error_callback,
				   conn);
  bufferevent_settimeout(conn->io, PROTOCOL_HEADER_TIMEOUT, 0);
  bufferevent_enable(conn->io, EV_READ | EV_WRITE);
  conn->state = CONNECTION_STATE_INITIAL;
  conn->vhost = AMQP_EMPTY_BYTES;

  info("Accepted connection from %s on fd %d", connection_name(conn));
}
