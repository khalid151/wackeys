EXEC=wackeys
SRC=$(EXEC).c
OBJ=$(EXEC).o
VERSION=1.1.1
CC=gcc
CFLAGS= -DVERSION="\"$(VERSION)\"" -DNAME="\"$(EXEC)\""
LIBS=-ludev -linput

PREFIX?=/usr
BINDIR=$(PREFIX)/bin

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(LIBS) $(OBJ) -o $(EXEC)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

debug: $(EXEC)
debug: CFLAGS += -g -Wall

clean:
	rm -rf *o $(EXEC)

install: $(EXEC) $(EXEC).1
	install -D -m 755 $(EXEC) $(DESTDIR)$(BINDIR)/$(EXEC)
	install -D -m 644 $(EXEC).1 $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(EXEC)
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1
