CC ?= $(CROSS_COMPILE)gcc # Allow overrides from Yocto
CFLAG ?= -Wall -Werror # Allow overrides from Yocto

.PHONY: clean
aesdsocket: aesdsocket.c
	$(CC) $(CFLAG) $^ -o $@

default: aesdsocket

all: default

clean:
	rm -f aesdsocket
