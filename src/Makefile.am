bin_PROGRAMS = rais

AM_CFLAGS = -I../../rabbitmq-c/librabbitmq
AM_LDFLAGS = -L../../rabbitmq-c/librabbitmq -lrabbitmq -ltokyocabinet

rais_SOURCES = \
	buffer.c util.c connection.c channel.c \
        buffer.h util.h connection.h channel.h \
	\
	exchange.c queue.c hashtable.c main.c \
	exchange.h queue.h hashtable.h rais.h \
	\
	syncpipe.c \
	syncpipe.h
