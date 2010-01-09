#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <netdb.h>

#include <amqp.h>

#include "rais.h"

void die(char const *format, ...) {
  va_list vl;
  va_start(vl, format);
  vfprintf(stderr, format, vl);
  va_end(vl);
  exit(1);
}

void warn(char const *format, ...) {
  va_list vl;
  va_start(vl, format);
  fprintf(stderr, "WARNING: ");
  vfprintf(stderr, format, vl);
  fputc('\n', stderr);
  va_end(vl);
}

void info(char const *format, ...) {
  va_list vl;
  va_start(vl, format);
  fprintf(stderr, "INFO: ");
  vfprintf(stderr, format, vl);
  fputc('\n', stderr);
  va_end(vl);
}

void get_addr_name(char *namebuf, size_t buflen, struct sockaddr_in const *sin) {
  unsigned char *addr = (unsigned char *) &sin->sin_addr.s_addr;
  struct hostent *h = gethostbyaddr(addr, 4, AF_INET);

  if (h == NULL) {
    snprintf(namebuf, buflen, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
  } else {
    snprintf(namebuf, buflen, "%s", h->h_name);
  }
}

void gensym(char *buf, size_t buflen, char const *prefix) {
  static int counter = 0;
  snprintf(buf, buflen, "%s%d", prefix, counter++);
}

amqp_bytes_t flatten_name(resource_name_t const *name) {
  amqp_bytes_t result = amqp_bytes_malloc(name->vhost.len + 1 + name->name.len);
  char *p = result.bytes;
  memcpy(p, name->vhost.bytes, name->vhost.len);
  p[name->vhost.len] = '\0';
  memcpy(&p[name->vhost.len + 1], name->name.bytes, name->name.len);
  return result;
}
