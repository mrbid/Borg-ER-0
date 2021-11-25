all:
	gcc main.c -Ofast -lSDL2 -lm -o borger0

install:
	cp borger0 $(DESTDIR)

uninstall:
	rm $(DESTDIR)/borger0

clean:
	rm borger0
