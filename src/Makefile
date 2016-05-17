.EXPORT_ALL_VARIABLES:

DESTDIR =
PREFIX = /usr/local

CFLAGS = -Wall
LDFLAGS =

CC = gcc
LD = gcc

RM = rm -f
INSTALL_DIR = install -m 0755 -d
INSTALL_BIN = install -m 0755

dynip-client: daemonize.o dynip-client.o
	$(LD) $(LDFLAGS) -o dynip-client daemonize.o dynip-client.o

dynip-server: daemonize.o dynip-server.o
	$(LD) $(LDFLAGS) -o dynip-server daemonize.o dynip-server.o

.c.o: config.h
	$(CC) $(CFLAGS) -c -o '$@' '$<'

.PHONY: client
client: dynip-client

.PHONY: server
server: dynip-server

.PHONY: all
all: client server

.PHONY: client_install
client_install:
	$(INSTALL_DIR) -- '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL_BIN) -- dynip-client '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: server_install
server_install:
	$(INSTALL_DIR) -- '$(DESTDIR)$(PREFIX)/bin'
	$(INSTALL_BIN) -- dynip-server '$(DESTDIR)$(PREFIX)/bin/'

.PHONY: install
install: client_install server_install

.PHONY: uninstall
uninstall: client_uninstall server_uninstall

.PHONY: client_uninstall
client_uninstall:
	$(RM) -- '$(DESTDIR)$(PREFIX)/bin/dynip-client'

.PHONY: server_uninstall
server_uninstall:
	$(RM) -- '$(DESTDIR)$(PREFIX)/bin/dynip-server'

.PHONY: uninstall
uninstall: client_uninstall server_uninstall

.PHONY: clean
clean:
	$(RM) -- *~ dynip-client dynip-server daemonize.o dynip-client.o dynip-server.o
