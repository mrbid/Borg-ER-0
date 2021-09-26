all:
	gcc main.c -Ofast -lSDL2 -lm -o borg0

install:
	cp borg0 $(DESTDIR)

uninstall:
	rm $(DESTDIR)/borg0

clean:
	rm borg0
