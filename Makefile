CC=gcc
CFLAGS=-ggdb -Wall

all: 

example_myshell: example_myshell.c
        $(CC) $(CFLAGS) -o $@ $<

clean:
        rm example_myshell
