CFLAGS = -O3 -lconfig -lm

all: $(OBJS)
	CC $(CFLAGS) -o cachesim main.c
debug: $(OBJS)
	CC $(CFLAGS) -ggdb -o cachesim main.c

clean:
	rm -rf *.o
	rm -rf *.dSYM

