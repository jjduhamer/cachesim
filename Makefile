CFLAGS = -O3 -lconfig -lm

all: main.c mycache.h
	CC $(CFLAGS) -o cachesim main.c
debug: main.c mycache.h
	CC $(CFLAGS) -ggdb -o cachesim main.c
stats: stats.c mycache.h
	CC $(CFLAGS) -o stats stats.c

clean:
	rm -rf *.dSYM

