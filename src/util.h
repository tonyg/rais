#ifndef rais_util_h
#define rais_util_h

extern void die(char const *format, ...);
extern void warn(char const *format, ...);
extern void info(char const *format, ...);

extern void get_addr_name(char *namebuf, size_t buflen, struct sockaddr_in const *addr);

extern void gensym(char *buf, size_t buflen, char const *prefix);

#endif
