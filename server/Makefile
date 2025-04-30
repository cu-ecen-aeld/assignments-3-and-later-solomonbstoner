CC ?= $(CROSS_COMPILE)gcc # Allow overrides from Yocto
CFLAGS ?= -Wall -Werror # Allow overrides from Yocto
LDFLAGS ?= 

.PHONY: clean
aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

default: aesdsocket

all: default

clean:
	rm -f aesdsocket
