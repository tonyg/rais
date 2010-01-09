#ifndef rais_connection_h
#define rais_connection_h

#define PROTOCOL_HEADER_TIMEOUT 10
#define MAX_CHANNELS 16
#define SUGGESTED_FRAME_MAX 65536

typedef enum connection_state_enum_ {
  CONNECTION_STATE_INITIAL,
  CONNECTION_STATE_HEADER_RECEIVED,
  CONNECTION_STATE_READY,
  CONNECTION_STATE_CLOSE_OK_WAIT,
  CONNECTION_STATE_DEAD
} connection_state_enum;

struct connstate_t_; /* forward */
struct chanstate_t_; /* forward */

typedef void (*channel_callback_t)(struct connstate_t_ *conn,
				   amqp_frame_t *frame,
				   struct chanstate_t_ *chan);

typedef struct connstate_t_ {
  struct sockaddr_in peername;
  int fd;
  amqp_connection_state_t amqp_conn;
  struct bufferevent *io;

  connection_state_enum state;
  channel_callback_t channel_callback[MAX_CHANNELS + 1];
  struct chanstate_t_ *channel_state[MAX_CHANNELS + 1];

  amqp_bytes_t vhost;
} connstate_t;

extern void start_inbound_connection(struct sockaddr_in const *peeraddr, int fd);
extern void close_connection(connstate_t *conn, short code, char const *explanation, ...);
extern void close_connection_command_invalid(connstate_t *conn, amqp_frame_t *frame);

#define ENSURE_FRAME_IS_METHOD(conn, receivedframe)		\
  if ((receivedframe)->frame_type != AMQP_FRAME_METHOD) {	\
    close_connection_command_invalid(conn, (receivedframe));	\
    return;							\
  } else {							\
  }

extern void set_channel_callback(connstate_t *conn,
				 amqp_channel_t ch,
				 channel_callback_t cb);

extern void close_channel(connstate_t *conn,
			  amqp_channel_t ch,
			  short code,
			  char const *explanation,
			  ...);

extern void send_method(connstate_t *conn,
			amqp_channel_t channel,
			amqp_method_number_t id,
			void *decoded);

#define SEND_METHOD(conn, ch, id, structname, ...)			\
  ({									\
    structname _decoded_method___ = (structname) { __VA_ARGS__ };	\
    send_method(conn, ch, id, &_decoded_method___);			\
  })

#endif
