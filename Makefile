############################################################
# shell Makefile
# by Ian Neal & Rob Kelly
#
# `make` to build SANIC SHEEL normally
# `make clean` to clean build files
# `make test` to run test cases
#
############################################################
CC = gcc
CFLAGS = -ggdb -Wall
VAL = valgrind --quiet --leak-check=yes --undef-value-errors=no --max-stackframe=10485760 --error-exitcode=1
OUT = shell
TESTS = test/*.in
T_SEQ = test/sequence*.in
T_CON = test/concurrent*.in
RM = rm -f

ECHO = echo -e
R_ALIGN = awk '{printf "%$(shell tput cols)s\n", $$1}'
FAIL_STRING = "\x1b[31;01m[FAILURE]\x1b[0m"
GOOD_STRING = "\x1b[32;01m[GOOD]\x1b[0m"

all: shell

shell: shell.c
	$(CC) $(CFLAGS) -o $(OUT) $<

test: shell $(TESTS)

test-sequential: shell $(T_SEQ)

test-concurrent: shell $(T_CON)

$(TESTS):
	@$(ECHO) "\t$(OUT) < $@"
	@$(VAL) ./$(OUT) < $@ || ($(ECHO) $(FAIL_STRING) | $(R_ALIGN); exit 1)
	@$(ECHO) $(GOOD_STRING) | $(R_ALIGN)
	@$(ECHO) "\t$(OUT) $@"
	@./$(OUT) $@ || ($(ECHO) $(FAIL_STRING) | $(R_ALIGN); exit 1)
	@$(ECHO) $(GOOD_STRING) | $(R_ALIGN)

clean:
	$(RM) $(OUT) *.o *~

.PHONY: $(TESTS)
