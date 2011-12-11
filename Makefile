CC = gcc
CFLAGS = -O3 -lconfig -lm
OBJS = main.o

all: $(OBJS)
	CC $(CFLAGS) -o cachesim $(OBJS)
	make clean
main.o: main.c mycache.h
	CC -c main.c

clean:
	rm $(OBJS)


