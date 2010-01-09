#ifndef rais_buffer_h
#define rais_buffer_h

typedef struct buffer_t_ {
  amqp_bytes_t buf;
  int pos;
} buffer_t;

extern void init_buffer(buffer_t *buf, size_t initial_length);
extern void destroy_buffer(buffer_t *buf);

extern amqp_bytes_t buf_contents(buffer_t *buf);

extern void buf_append(buffer_t *buf, char ch);
extern void buf_append_many(buffer_t *buf, amqp_bytes_t chs);

extern void buf_insert(buffer_t *buf, char ch, int pos);

extern void buf_delete(buffer_t *buf, int pos);

#endif
