GCC = /usr/bin/gcc
CFLAGS = -Wall -std=gnu99 -pthread

myserver: myserver.c
	$(GCC) $(CFLAGS) -o $@ $<
cclient: cclient.c
	$(GCC) $(CFLAGS) -o $@ $<

.PHONY: clean

clean:
	$(RM) -r myserver cclient
