GCC = /usr/bin/gcc
CFLAGS ?= -std=gnu99 -pthread

myserver: myserver.c
	$(GCC) $(CFLAGS) -c $< -o $@
cclient: cclient.c
	$(GCC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	$(RM) -r myserver cclient
