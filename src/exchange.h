#ifndef rais_exchange_h
#define rais_exchange_h

typedef struct exchange_type_t_ exchange_type_t;
typedef struct exchange_t_ exchange_t;

struct exchange_type_t_ {
  amqp_bytes_t name;
  void (*init)(exchange_t *x);
  void (*destroy)(exchange_t *x);
};

struct exchange_t_ {
  resource_name_t name;
  exchange_type_t *type;
  void *type_data;
  amqp_boolean_t durable;
  amqp_boolean_t auto_delete;
  amqp_table_t arguments;
};

extern void init_exchange(void);
extern void done_exchange(void);

extern void register_exchange_type(exchange_type_t *type);
extern exchange_type_t *lookup_exchange_type(amqp_bytes_t name);

extern exchange_t *declare_exchange(int *status, /* out */
				    resource_name_t name,
				    exchange_type_t *type,
				    amqp_boolean_t durable,
				    amqp_boolean_t auto_delete,
				    amqp_table_t arguments);
extern exchange_t *lookup_exchange(int *status, /* out */
				   resource_name_t name);
extern void delete_exchange(int *status, /* out */
			    exchange_t *x);

#endif
