#ifndef rais_exchange_h
#define rais_exchange_h

typedef struct exchange_type_t_ exchange_type_t;
typedef struct exchange_t_ exchange_t;

struct exchange_type_t_ {
  amqp_bytes_t name;
  void (*init)(exchange_t *x);
  void (*destroy)(exchange_t *x);
  void (*bind)(int *status, exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments);
  void (*unbind)(int *status, exchange_t *x, queue_t *q, amqp_bytes_t rk, amqp_table_t arguments);
};

struct exchange_t_ {
  amqp_bytes_t name;
  exchange_type_t *type;
  void *type_data;
  amqp_boolean_t durable;
  amqp_boolean_t auto_delete;
  amqp_table_t arguments;

  /* Routes */
  hashtable_t fanout; /* qname -> queue_t*, but the qname is just a unique tag */
  hashtable_t direct; /* routing_key -> qname -> queue_t*, again qname is just a tag */
};

extern void init_exchange(void);
extern void done_exchange(void);

extern void register_exchange_type(exchange_type_t *type);
extern exchange_type_t *lookup_exchange_type(amqp_bytes_t name);

extern exchange_t *declare_exchange(int *status, /* out */
				    vhost_t *vhost,
				    amqp_bytes_t name,
				    exchange_type_t *type,
				    amqp_boolean_t durable,
				    amqp_boolean_t auto_delete,
				    amqp_table_t arguments);
extern exchange_t *lookup_exchange(int *status, /* out */
				   vhost_t *vhost,
				   amqp_bytes_t name);
extern void delete_exchange(int *status, /* out */
			    exchange_t *x);

extern void exchange_bind(int *status, /* out */
			  exchange_t *x,
			  queue_t *q,
			  amqp_bytes_t routing_key,
			  amqp_table_t arguments);
extern void exchange_unbind(int *status, /* out */
			    exchange_t *x,
			    queue_t *q,
			    amqp_bytes_t routing_key,
			    amqp_table_t arguments);

extern syncpipe_out_t exchange_route(int *status, /* out */
				     exchange_t *x,
				     amqp_bytes_t routing_key,
				     amqp_basic_properties_t *props,
				     uint64_t body_size,
				     amqp_bytes_t raw_props);

#endif
