CC=gcc
CFLAG=-Wall -Werror

.PHONY: clean
aesdsocket: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAG) $^ -o $@

default: aesdsocket

all: default

clean:
	rm -f aesdsocket
