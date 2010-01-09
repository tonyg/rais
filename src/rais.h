#ifndef rais_rais_h
#define rais_rais_h

typedef struct resource_name_t_ {
  amqp_bytes_t vhost;
  amqp_bytes_t name;
} resource_name_t;

amqp_bytes_t flatten_name(resource_name_t const *name);

#endif
