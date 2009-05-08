#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <stdint.h>
#include <amqp.h>
#include <amqp_framing.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>

#include <stdarg.h>
#include <assert.h>

#include <event.h>

static short listen_port = 5672;

static struct event accept_event;

typedef struct connstate_t_ {
  int fd;
  struct event read_event;
  struct timeval read_timeout;
} connstate_t;

static void die(char const *format, ...) {
  va_list vl;
  va_start(vl, format);
  vfprintf(stderr, format, vl);
  va_end(vl);
  exit(1);
}

static void warn(char const *format, ...) {
  va_list vl;
  va_start(vl, format);
  fprintf(stderr, "WARNING: ");
  vfprintf(stderr, format, vl);
  fputc('\n', stderr);
  va_end(vl);
}

static void info(char const *format, ...) {
  va_list vl;
  va_start(vl, format);
  fprintf(stderr, "INFO: ");
  vfprintf(stderr, format, vl);
  fputc('\n', stderr);
  va_end(vl);
}

static void usage(void) {
  die("Usage: rais [-p <listenport>]\n");
}

static void parse_options(int argc, char * const argv[]) {
  while (1) {
    switch (getopt(argc, argv, "p:")) {
      case 'p':
	listen_port = (short) atoi(optarg);
	break;

      case EOF:
	return;

      default:
	usage();
    }
  }
}

static int create_server_socket(void) {
  int serv = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in s;

  if (serv < 0) {
    die("Could not open listen socket.\n");
  }

  s.sin_family = AF_INET;
  s.sin_addr.s_addr = htonl(INADDR_ANY);
  s.sin_port = htons(listen_port);

  {
    int i = 1; // 1 == turn on the option
    setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)); // don't care if this fails
  }

  if (bind(serv, (struct sockaddr *) &s, sizeof(s)) < 0) {
    die("Could not bind listen socket.\n");
  }

  if (listen(serv, 5) < 0) {
    int savedErrno = errno;
    die("Could not listen on listen socket (errno %d: %s).\n",
	savedErrno, strerror(savedErrno));
  }

  return serv;
}

static void handle_inbound_data(int fd, short what, connstate_t *conn) {
  char buf[10];
  int nread = 9999;

  printf("%d %d\n", conn->fd, what);
  switch (what) {
    case EV_TIMEOUT:
      printf("timeout! %d %lu %lu\n", fd,
	     (unsigned long) conn->read_timeout.tv_sec,
	     (unsigned long) conn->read_timeout.tv_usec);
      write(fd, "-----\n", 6);
      event_add(&conn->read_event, &conn->read_timeout);
      break;

    case EV_READ:
      nread = read(fd, buf, sizeof(buf));

      if (nread > 0) {
	write(fd, buf, nread);
	printf("xfer %d\n", nread);
      }

      if (nread == 0) {
	printf("closing %d\n", fd);
	event_del(&conn->read_event);
	close(fd);
	free(conn);
      }
      break;

    default:
      printf("huh??\n");
      break;
  }

  printf("done %d %d\n", fd, nread);
}

static void setup_inbound_connection(int fd) {
  connstate_t *conn = calloc(sizeof(connstate_t), 1);
  info("setup_inbound_connection(%d)", fd);

  conn->fd = fd;
  event_set(&conn->read_event, fd, EV_READ | EV_PERSIST,
	    (void (*)(int,short,void*)) handle_inbound_data,
	    conn);
  conn->read_timeout.tv_sec = 2;
  conn->read_timeout.tv_usec = 0;
  event_add(&conn->read_event, &conn->read_timeout);
}

static void accept_connection(int servfd, short what, void *arg) {
  struct sockaddr_in s;
  socklen_t addrlen = sizeof(s);
  int fd = accept(servfd, (struct sockaddr *) &s, &addrlen);

  if (fd == -1) {
    if (errno != EAGAIN && errno != EINTR) {
      warn("Accept resulted in errno %d: %s", errno, strerror(errno));
    }
    return;
  }

  info("Got connection %d", fd);
  setup_inbound_connection(fd);
}

static void setup_server_socket(void) {
  int servfd = create_server_socket();

  event_set(&accept_event, servfd, EV_READ | EV_PERSIST, accept_connection, NULL);
  if (event_add(&accept_event, NULL) == -1) {
    die("Could not add accept_event.");
  }
}

int main(int argc, char * const argv[]) {
  event_init();
  parse_options(argc, argv);
  setup_server_socket();
  event_dispatch();
  return 0;
}
