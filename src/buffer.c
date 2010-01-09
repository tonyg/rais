#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netdb.h>

#include <stdint.h>
#include <amqp.h>

#include "util.h"
#include "buffer.h"

void init_buffer(buffer_t *buf, size_t initial_length) {
  buf->buf = amqp_bytes_malloc(initial_length);
  buf->pos = 0;
}

void destroy_buffer(buffer_t *buf) {
  AMQP_BYTES_FREE(buf->buf);
}

amqp_bytes_t buf_contents(buffer_t *buf) {
  amqp_bytes_t result;
  result.len = buf->pos;
  result.bytes = buf->buf.bytes;
  return result;
}

static void grow_buf(buffer_t *buf, size_t delta) {
  char *newbuf = malloc(buf->buf.len + delta);
  if (newbuf == NULL) {
    die("grow_buf: could not grow buffer");
  }
  memcpy(newbuf, buf->buf.bytes, buf->buf.len);
  free(buf->buf.bytes);
  buf->buf.bytes = newbuf;
  buf->buf.len += delta;
}

void buf_append(buffer_t *buf, char ch) {
  if (buf->pos >= buf->buf.len) {
    grow_buf(buf, 128);
  }
  ((char *) buf->buf.bytes)[buf->pos++] = ch;
}

void buf_append_many(buffer_t *buf, amqp_bytes_t chs) {
  if ((buf->pos + chs.len) > buf->buf.len) {
    grow_buf(buf, chs.len + 128);
  }
  memcpy(((char *) buf->buf.bytes) + buf->pos, chs.bytes, chs.len);
  buf->pos += chs.len;
}

void buf_insert(buffer_t *buf, char ch, int pos) {
  char *p = buf->buf.bytes;

  if (pos < 0)
    pos = 0;
  if (pos > buf->pos)
    pos = buf->pos;

  buf_append(buf, 0);
  memmove(&p[pos + 1], &p[pos], buf->pos - pos - 1);
  p[pos] = ch;
}

void buf_delete(buffer_t *buf, int pos) {
  char *p = buf->buf.bytes;

  if (pos < 0)
    pos = 0;
  if (pos >= buf->pos)
    pos = buf->pos - 1;

  memmove(&p[pos], &p[pos + 1], buf->pos - pos - 1);
  buf->pos--;
}
