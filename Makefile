CC = gcc
CLFAGS = -g -std=c99 -Wall -Wextra -Werror -Wfatal-errors -pedantic
LDLIBS = -lm -lrt -lpthread

all: a1

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

a1: timing.o
	$(CC) $^ -o $@ $(LDLIBS)

clean:
	rm -f a1 *.o *.out