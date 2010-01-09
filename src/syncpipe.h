#ifndef rais_syncpipe_h
#define rais_syncpipe_h

typedef void (*syncpipe_callback_t)(void *context, amqp_bytes_t data);

typedef struct syncpipe_out_chunk_t_ {
  struct syncpipe_out_chunk_t_ *next;
  amqp_bytes_t data;
  size_t remaining;
  void *context;
  syncpipe_callback_t callback;
} syncpipe_out_chunk_t;

typedef struct syncpipe_reader_t_ {
  struct syncpipe_reader_t_ *next;
  size_t remaining;
  void *context;
  syncpipe_callback_t callback;
} syncpipe_reader_t;

typedef struct syncpipe_t_ {
  syncpipe_out_chunk_t *first_out;
  syncpipe_out_chunk_t *last_out;
  syncpipe_reader_t *first_in;
  syncpipe_reader_t *last_in;
  amqp_boolean_t is_closed;
} syncpipe_t;

extern syncpipe_t *open_syncpipe(void);
extern void syncpipe_write(syncpipe_t *p,
			   amqp_bytes_t data,
			   void *context,
			   syncpipe_callback_t callback);
extern void syncpipe_read(syncpipe_t *p,
			  size_t length,
			  void *context,
			  syncpipe_callback_t callback);
extern void syncpipe_close(syncpipe_t *p);

#endif
