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

#include "util.h"
#include "hashtable.h"
#include "syncpipe.h"
#include "vhost.h"
#include "connection.h"
#include "queue.h"
#include "exchange.h"

struct rais_configuration_t_ {
  short listen_port;
} cfg;

static struct event accept_event;

static void usage(void) {
  die("Usage: rais [-p <listenport>]\n");
}

static void parse_options(int argc, char * const argv[]) {
  memset(&cfg, 0, sizeof(cfg));
  cfg.listen_port = 5672;

  while (1) {
    switch (getopt(argc, argv, "p:")) {
      case 'p':
	cfg.listen_port = (short) atoi(optarg);
	break;

      case EOF:
	return;

      default:
	usage();
    }
  }
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

  start_inbound_connection(&s, fd);
}

static void setup_server_socket(void) {
  int servfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in s;

  if (servfd < 0) {
    die("Could not open listen socket.\n");
  }

  s.sin_family = AF_INET;
  s.sin_addr.s_addr = htonl(INADDR_ANY);
  s.sin_port = htons(cfg.listen_port);

  {
    int i = 1; // 1 == turn on the option
    setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)); // don't care if this fails
  }

  if (bind(servfd, (struct sockaddr *) &s, sizeof(s)) < 0) {
    die("Could not bind listen socket.\n");
  }

  if (listen(servfd, 5) < 0) {
    int savedErrno = errno;
    die("Could not listen on listen socket (errno %d: %s).\n",
	savedErrno, strerror(savedErrno));
  }

  event_set(&accept_event, servfd, EV_READ | EV_PERSIST, accept_connection, NULL);
  if (event_add(&accept_event, NULL) == -1) {
    die("Could not add accept_event.");
  }

  info("Accepting connections on port %d.", cfg.listen_port);
}

int main(int argc, char * const argv[]) {
  event_init();
  parse_options(argc, argv);

  init_vhost();
  init_exchange();
  init_queue();

  declare_vhost(amqp_cstring_bytes("/"));

  setup_server_socket();

  event_dispatch();

  done_queue();
  done_exchange();
  done_vhost();
  return 0;
}
