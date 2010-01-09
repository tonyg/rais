bin_PROGRAMS = rais

AM_CFLAGS = -I../../rabbitmq-c/librabbitmq
AM_LDFLAGS = -L../../rabbitmq-c/librabbitmq -lrabbitmq -ltokyocabinet

rais_SOURCES = util.c connection.c channel.c exchange.c queue.c hashtable.c main.c \
               util.h connection.h channel.h exchange.h queue.h hashtable.h rais.h
