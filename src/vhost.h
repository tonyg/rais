#ifndef rais_vhost_h
#define rais_vhost_h

typedef struct vhost_t_ {
  amqp_bytes_t name;
  hashtable_t exchanges;
  hashtable_t queues;
} vhost_t;

extern void init_vhost(void);
extern void done_vhost(void);

extern vhost_t *declare_vhost(amqp_bytes_t name);
extern vhost_t *lookup_vhost(amqp_bytes_t name);

#endif
