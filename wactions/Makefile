EXEC=wactions.py
NAME=wactions
PREFIX?=/usr
BINDIR=$(PREFIX)/bin

install:
	install -D -m 755 $(EXEC) $(DESTDIR)$(BINDIR)/$(NAME)
	install -D -m 644 config /etc/xdg/$(NAME)/config

uninstall:
	rm -f $(DESTDIR)/$(BINDIR)/$(NAME)
	rm -f /etc/xdg/$(NAME)/config
