bin_PROGRAMS = rais

AM_CFLAGS = -I../../rabbitmq-c/librabbitmq
AM_LDFLAGS = -L../../rabbitmq-c/librabbitmq -lrabbitmq -ltokyocabinet

rais_SOURCES = \
	buffer.c util.c connection.c channel.c \
        buffer.h util.h connection.h channel.h \
	\
	vhost.c exchange.c queue.c hashtable.c main.c \
	vhost.h exchange.h queue.h hashtable.h \
	\
	exchange_direct.c exchange_fanout.c exchange_topic.c \
	\
	syncpipe.c \
	syncpipe.h
