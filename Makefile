EXEC=wackeys
SRC=$(EXEC).c
OBJ=$(EXEC).o
VERSION=1.2.1
CC=gcc
CFLAGS= -DVERSION="\"$(VERSION)\"" -DNAME="\"$(EXEC)\"" -Wall
LIBS=-ludev -linput

PREFIX?=/usr
BINDIR=$(PREFIX)/bin

all: CFLAGS += -Werror
all: $(EXEC)
	@echo -e	"[Unit]\n\
	Description=Wackeys daemon service\n\
	Documentation=man:wackeys(1)\n\
	After=systemd-udevd.service\n\
	\n\
	[Service]\n\
	Type=simple\n\
	ExecStart=$(DESTDIR)$(BINDIR)/$(EXEC)\n\
	\n\
	[Install]\n\
	WantedBy=multi-user.target" > $(EXEC).service

$(EXEC): $(OBJ)
	$(CC) $(LIBS) $(OBJ) -o $(EXEC)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

debug: $(EXEC)
debug: CFLAGS += -g

clean:
	rm *o $(EXEC) $(EXEC).service

install: $(EXEC) $(EXEC).1
	install -D -m 755 $(EXEC) $(DESTDIR)$(BINDIR)/$(EXEC)
	install -D -m 644 $(EXEC).1 $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1

install_service:
	install -D -m 644 $(EXEC).service $(DESTDIR)$(PREFIX)/lib/systemd/system/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(EXEC)
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(EXEC).1

uninstall_service:
	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/system/$(EXEC).service
