/*******************************************************************************
/
/ filename: myshell.c
/ description: A very basic shell that allows commands to be run in the
/   foreground or background, but does not have any common shell builtins,
/   besides "exit".  There are also no shell variables, and no return status of
/   executed foreground commands is available.
/ author: Neal, Ian
/ class: CSE 222 S14
/ instructor: Zheng
/ assignment: Project #3
/ assigned: Mar. 3, 2014
/ due: Mar. 10, 2014
/
*******************************************************************************/

// *INCLUDES*
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


// *MACROS
#define sne(x,y) (strcmp((x),(y))) // String inequality
#define seq(x,y) (!sne((x),(y))) // Functional definition of string equality
#define str_last(x) x[strlen(x) - 1] // Not to be used with expressions


// *CONSTANTS*
#define DELIMITER " " // The delimiter between arguments
#define isnot ! // Make the language more readable
#define MAX_LINE 80 // The maximum number of characters to allow for input
#define PS1 "cse222> " // The default prompt; $PS1 is a shell's prompt variable
#define SHELL_NAME "myshell" // The name of the shell; to be printed on errors


// *TYPEDEFS AND GLOBALS*
typedef enum { false, true } bool;


// *FUNCTION PROTOTPYES*
void child_handler(int signum);
void execute(char **args, bool bg);
void free_args(char **args);
char *mystrcpy(char *src, size_t size);
void parse_input(char *buf, int *argc, char **argv);
bool strip_bg(int *argc, char **argv);
void strip_newline(char *str);


// *MAIN PROGRAM*
int main(int argc, char **argv)
{
  int myargc = 0;
  char *myargv[MAX_LINE/2 + 1] = {NULL}; // Null-terminated string array? Please!
  char buf[MAX_LINE];
  bool should_run = true;
  bool bg;

  signal(SIGCHLD, child_handler);
    // According to the man-page, signal() is not portable, and sigaction()
    // should be used instead; however, I have NO idea how to use sigaction()
    // and I DO know how to use signal() for basic things.  I'm sure we'll go
    // over sigaction() in class later.

  while(should_run) {
    printf(PS1);
    fflush(stdout); // Flush output from last run before starting new run
    fgets(buf, MAX_LINE, stdin);
    strip_newline(buf);
    if (seq(buf, "exit")) {
      should_run = false;
    } else if(strlen(buf)) { // Only exec if entry is not just "\n"
      parse_input(buf, &myargc, myargv);
      bg = strip_bg(&myargc, myargv);
      execute(myargv, bg);
      free_args(myargv);
      myargc = 0;
    }
  }

  return 0;
}

// Print when a child exits; a signal handler for SIGCHLD
// Got a hint for this from http://stackoverflow.com/questions/13320159
void child_handler(int signum)
{
  int exitstat;
  int pid;
  while((pid = waitpid(-1, &exitstat, WNOHANG)) > 0) {
    printf("Background child (pid %i) exited with status %i\n", pid, exitstat);
  }
}

// Execute the command, and run in the background if bg is true
void execute(char **args, bool bg)
{
  pid_t pid;
  pid = fork();
  if(pid == 0) {
    if(execvp(args[0], args) < 0) { // An error occured with the exec'd program
      perror(SHELL_NAME);
    }
    exit(0);
  } else if(pid < 0){ // An error occured fork()ing
    perror(SHELL_NAME);
  } else if(isnot bg) { // Gonna wait
    waitpid(pid, NULL, 0);
  }
}

// After the args are used, free each.
void free_args(char **args)
{
  int i;
  for(i=0; args[i] != NULL && i <= MAX_LINE / 2; i++) {
    free(args[i]);
    args[i] = NULL;
  }
}

// A safer string-copy that also ensures null-termination. Creates copy buffer.
// Note that the pointer returned should be freed after its use.
// Based on https://www.sourceware.org/ml/libc-alpha/2000-08/msg00061.html
// That link is wrong, though
char *mystrcpy(char *src, size_t size)
{
  char *dest = malloc(size * sizeof(char));
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
  return dest;
}

// Transfer the DELIMITER-separated string in buf to separate entries in args
void parse_input(char *buf, int *argc, char **argv)
{
  char *bufcpy = NULL;
  char *token = NULL;
  bufcpy = mystrcpy(buf, MAX_LINE); // strtok() modifies the original
  token = strtok(bufcpy, DELIMITER);
  argv[0] = mystrcpy(token, strlen(token) + 1); // Needs room for the NULL
  for((*argc)++; (token = strtok(NULL, DELIMITER)); (*argc)++) {
    argv[*argc] = mystrcpy(token, strlen(token) + 1);
  }
}

// Strips the "&" from the end of argv and returns true, if present. Otherwise,
// returns false.
bool strip_bg(int *argc, char **argv)
{
  bool bg = false;
  if(seq(argv[*argc - 1], "&")) {
    bg = true;
    free(argv[*argc - 1]);
    argv[*argc] = NULL;
    (*argc)--;
  }
  return bg;
}

// Strips a newline from the end of a string, if it exists.  Useful for dealing
// with strings from stdin.
void strip_newline(char *str)
{
  if(str_last(str) == '\n')
    str_last(str) = '\0';
}
