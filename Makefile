############################################################
# shell Makefile
# by Ian Neal & Rob Kelly
#
# `make` to build SANIC SHEEL normally
# `make clean` to clean build files
#
############################################################
CC = gcc
CFLAGS = -ggdb -Wall
OUT = shell
RM = rm -f

all: shell

shell: shell.c
	$(CC) $(CFLAGS) -o $(OUT) $<

clean:
	$(RM) $(OUT) *.o *~
