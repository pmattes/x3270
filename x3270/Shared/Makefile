# Makefile for shared objects

bootstrap: favicon.c

favicon.c: favicon.ico mkicon
	./mkicon <favicon.ico >favicon.c

mkicon: mkicon.c
	$(CC) $(CFLAGS) -o mkicon mkicon.c
