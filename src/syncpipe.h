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

typedef struct syncpipe_in_t_ {
  syncpipe_t *p;
} syncpipe_in_t;

typedef struct syncpipe_out_t_ {
  syncpipe_t *p;
} syncpipe_out_t;

#define NULL_SYNCPIPE_OUT ((syncpipe_out_t) { .p = NULL })
#define NULL_SYNCPIPE_IN ((syncpipe_in_t) { .p = NULL })

extern void open_syncpipe(syncpipe_out_t *po, syncpipe_in_t *pi);
extern amqp_boolean_t syncpipe_write(syncpipe_out_t po,
				     amqp_bytes_t data,
				     void *context,
				     syncpipe_callback_t callback);
extern amqp_boolean_t syncpipe_read(syncpipe_in_t pi,
				    size_t length,
				    void *context,
				    syncpipe_callback_t callback);
extern void syncpipe_close_out(syncpipe_out_t po);
extern void syncpipe_close_in(syncpipe_in_t pi);

#endif
