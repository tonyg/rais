#ifndef rais_queue_h
#define rais_queue_h

typedef struct queue_t_ {
  amqp_bytes_t name;
  amqp_boolean_t durable;
  amqp_boolean_t auto_delete;
  amqp_table_t arguments;
  long queue_len;
  int consumer_count;
} queue_t;

extern void init_queue(void);
extern void done_queue(void);

extern queue_t *declare_queue(int *status, /* out */
			      vhost_t *vhost,
			      amqp_bytes_t name,
			      amqp_boolean_t durable,
			      amqp_boolean_t auto_delete,
			      amqp_table_t arguments);
extern queue_t *lookup_queue(int *status, /* out */
			     vhost_t *vhost,
			     amqp_bytes_t name);
extern void delete_queue(int *status, /* out */
			 queue_t *x);

#endif
