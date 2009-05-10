#ifndef rais_channel_h
#define rais_channel_h

typedef struct chanstate_t_ {
  amqp_channel_t channel;
} chanstate_t;

extern chanstate_t *new_channel_state(amqp_channel_t channel);
extern void destroy_channel_state(chanstate_t *chan);

extern void handle_channel_normal(connstate_t *conn, amqp_frame_t *frame, chanstate_t *chan);

#endif
