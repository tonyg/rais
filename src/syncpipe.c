#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netdb.h>
#include <assert.h>

#include <stdint.h>
#include <amqp.h>

#include "syncpipe.h"

syncpipe_t *open_syncpipe(void) {
  syncpipe_t *p = malloc(sizeof(syncpipe_t));
  p->first_out = NULL;
  p->last_out = NULL;
  p->first_in = NULL;
  p->last_in = NULL;
  p->is_closed = 0;
  return p;
}

static void run_syncpipe(syncpipe_t *p) {
  syncpipe_out_chunk_t *w = p->first_out;
  syncpipe_reader_t *r = p->first_in;

  while (w != NULL && r != NULL) {
    size_t amount = (r->remaining > w->remaining) ? w->remaining : r->remaining;
    size_t offset = w->data.len - w->remaining;
    amqp_bytes_t block;

    assert(amount > 0);

    block.bytes = ((char *) w->data.bytes) + offset;
    block.len = amount;
    r->callback(r->context, block);

    r->remaining -= amount;
    w->remaining -= amount;

    if (r->remaining == 0) {
      p->first_in = r->next;
      if (p->first_in == NULL) p->last_in = NULL;
      free(r);
    }

    if (w->remaining == 0) {
      if (w->callback) {
	w->callback(w->context, w->data);
      }
      p->first_out = w->next;
      if (p->first_out == NULL) p->last_out = NULL;
      free(w);
    }
  }
}

void syncpipe_write(syncpipe_t *p,
		    amqp_bytes_t data,
		    void *context,
		    syncpipe_callback_t callback)
{
  syncpipe_out_chunk_t *w = malloc(sizeof(syncpipe_out_chunk_t));
  w->next = NULL;
  w->data = data;
  w->remaining = data.len;
  w->context = context;
  w->callback = callback;
  if (p->last_out != NULL) {
    p->last_out->next = w;
  } else {
    p->first_out = w;
  }
  p->last_out = w;
  run_syncpipe(p);
}

void syncpipe_read(syncpipe_t *p,
		   size_t length,
		   void *context,
		   syncpipe_callback_t callback)
{
  syncpipe_reader_t *r = malloc(sizeof(syncpipe_reader_t));
  r->next = NULL;
  r->remaining = length;
  r->context = context;
  r->callback = callback;
  if (p->last_in != NULL) {
    p->last_in->next = r;
  } else {
    p->first_in = r;
  }
  p->last_in = r;
  run_syncpipe(p);
}

void syncpipe_close(syncpipe_t *p) {
  p->is_closed = 1;

  {
    syncpipe_out_chunk_t *w = p->first_out;
    while (w != NULL) {
      syncpipe_out_chunk_t *next = w->next;
      if (w->callback) {
	w->callback(w->context, w->data);
      }
      free(w);
      w = next;
    }
  }

  {
    syncpipe_reader_t *r = p->first_in;
    while (r != NULL) {
      syncpipe_reader_t *next = r->next;
      r->callback(r->context, AMQP_EMPTY_BYTES);
      free(r);
      r = next;
    }
  }

  free(p);
}
