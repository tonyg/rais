TARGETS = uc pthtest
#CFLAGS = -Wall -O0 -g
CFLAGS = -Wall -O3

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< -lpth

clean:
	rm -f $(TARGETS)
	rm -rf *.dSYM
