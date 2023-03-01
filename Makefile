CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lm

SRCS = $(sort $(wildcard *.c))
TARGETS = $(patsubst %.c,%,$(SRCS))

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *.o $(TARGETS)
