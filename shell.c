/*******************************************************************************
 *
 * filename: shell.c
 *
 * description: A very basic shell that allows commands to be run in the
 *              foreground or background, can run multiple commands
 *              concurrently, and can read a batch file of commands and run them
 *              each sequentially. No shell variables are available, and no
 *              built-in shell functions exist besides `quit`.
 *
 * authors: Ian Neal, Rob Kelly
 * class: CSE 325
 * instructor: Zheng
 * assignment: Lab Project 2
 * assigned: Jan 18 2015
 * due: Feb 11 2015
 ******************************************************************************/

// *INCLUDES*
#include <errno.h> // perror
#include <signal.h> // signal, SIGCHLD
#include <stdio.h> // printf, fflush, fgets,
#include <stdlib.h> // exit, free, malloc
#include <string.h> // strncpy, strlen, strtok, strcmp
#include <sys/wait.h> // waitpid, WNOHANG
#include <unistd.h> // fork, execvp
#include <stdbool.h> // bool, true, false

// *CONSTANTS*
#define DELIMITER " " // The delimiter between arguments
#define MAX_LINE 80 // The maximum number of characters to allow for input
#define PS1 "SANIC TEEM> " // The default prompt; $PS1 is the prompt variable
#define ERR_MSG "shell: Error" // Error message, for perror

// *FUNCTION PROTOTPYES*
void child_handler(int signum);
void execute(char **args, bool bg);
void free_args(char **args);
char *mystrcpy(char *src, size_t size);
void parse_input(char *buf, int *argc, char **argv);
bool strip_bg(int *argc, char **argv);
void strip_newline(char *str, size_t len);
int run_interactive();
int run_batch(char *fname);
int read_line(FILE *stream, char *buf);
bool shell_repl(char *buf, size_t buf_len);

// *MAIN PROGRAM*
int main(int argc, char **argv) {

  // According to the man-page, signal() is not portable, and sigaction()
  // should be used instead; however, I have NO idea how to use sigaction()
  // and I DO know how to use signal() for basic things.  I'm sure we'll go
  // over sigaction() in class later.
  signal(SIGCHLD, child_handler);

  switch(argc) {
    case 1: // Invoked with no arguments, run interactive mode
      return run_interactive();
      break;

    case 2: // Given one argument, treat it as a filename and run batch mode
      return run_batch(argv[1]);
      break;

    default: // Given weird arguments. Print usage and exit.
      errno = EINVAL;
      perror(ERR_MSG);
      printf("Usage:\n");
      printf("\t%s\n", argv[0]);
      printf("\t%s [BATCHFILE]\n", argv[0]);

      return -1;
  }
}

/**
 * Print when a child exits; a signal handler for SIGCHLD.
 * Got a hint for this from http://stackoverflow.com/questions/13320159
 *
 * @param signum  Signal whose behavior is handled. Unused.
 */
void child_handler(int signum) {
  int exitstat;
  int pid;
  while((pid = waitpid(-1, &exitstat, WNOHANG)) > 0) {
    printf("Background child (pid %i) exited with status %i\n", pid, exitstat);
  }
}

/**
 * Execute the command, and run in the background if bg is true.
 *
 * @param args  Array of strings. First element is the program to run,
 *              the rest of the array are the arguments.
 * @param bg    If true, the command is run in the background.
 */
void execute(char **args, bool bg) {
  pid_t pid;
  pid = fork();
  if(pid == 0) {
    if(execvp(args[0], args) < 0) { // An error occured with the exec'd program
      perror(ERR_MSG);
    }
    exit(0);
  } else if(pid < 0){ // An error occured fork()ing
    perror(ERR_MSG);
  } else if(!bg) { // Gonna wait
    waitpid(pid, NULL, 0);
  }
}

/**
 * After the args are used, free each.
 *
 * @param args  Array of strings. Each will be freed.
 */
void free_args(char **args) {
  int i;
  for(i=0; args[i] != NULL && i <= MAX_LINE / 2; i++) {
    free(args[i]);
    args[i] = NULL;
  }
}

/**
 * A safer string-copy that also ensures null-termination. Creates copy buffer.
 * Note that the pointer returned should be freed after its use.
 * Based on https://www.sourceware.org/ml/libc-alpha/2000-08/msg00061.html
 * That link is wrong, though
 *
 * @param src   Original string to be copied.
 * @param size  Length of source string.
 * @return      A copy of the source string, in newly allocated memory.
 */
char *mystrcpy(char *src, size_t size) {
  char *dest = malloc(size * sizeof(char));
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
  return dest;
}

/**
 * Transfer the DELIMITER-separated string in buf to separate entries in args
 *
 * @param buf   A line taken from input with a command and list of arguments.
 * @param argc  Pointer to argument counter. On return, contains the number of
 *              arguments in the line.
 * @param argv  Empty array of strings. On return, contains a representation of
 *              the command as a list of arguments. Each element must be freed
 *              by the caller.
 */
void parse_input(char *buf, int *argc, char **argv) {
  char *bufcpy = NULL;
  char *token = NULL;
  bufcpy = mystrcpy(buf, MAX_LINE); // strtok() modifies the original
  token = strtok(bufcpy, DELIMITER);
  argv[0] = mystrcpy(token, strlen(token) + 1); // Needs room for the NULL
  for((*argc)++; (token = strtok(NULL, DELIMITER)); (*argc)++) {
    argv[*argc] = mystrcpy(token, strlen(token) + 1);
  }
  free(bufcpy);
}

/**
 * Strips the "&" from the end of argv and returns true, if present.
 *
 * @param argc  Length of argument list.
 * @param argv  List of arguments to be filtered.
 * @return      True if an `&` was stripped from the argument list,
 *              false otherwise.
 */
bool strip_bg(int *argc, char **argv) {
  bool bg = false;
  if(!strcmp(argv[*argc - 1], "&")) {
    bg = true;
    free(argv[*argc - 1]);
    argv[*argc] = NULL;
    (*argc)--;
  }
  return bg;
}

/**
 * Strips a newline from the end of a string, if it exists.  Useful for dealing
 * with strings from stdin.
 *
 * @param str  A string.
 * @param len  Index of the last character in the string (length - 1).
 */
void strip_newline(char *str, size_t len) {
  if(str[len] == '\n')
    str[len] = '\0';
}

/**
 * When invoked without a filename argument, the shell runs in interactive mode,
 * and takes input line by line from stdin. The shell also prints a prompt for
 * the user.
 */
int run_interactive() {
  char buf[MAX_LINE]; // Line buffer
  int buf_len;
  do {
    printf(PS1);

    // Read line, handle bad return status (<= 0)
    if((buf_len = read_line(stdin, buf)) <= 0) {
      return buf_len;
    }
    
  } while(shell_repl(buf, buf_len - 1));

  return 0;
}

/**
 * When given a filename, the shell runs in batch mode, taking input from each
 * line in the file sequentially. No prompt is displayed.
 *
 * @param fname  Path of a file from which to read input.
 */
int run_batch(char* fname) {
  // TODO: this thing
  return 0;
}

/**
 * Read a line from the input stream and handle special cases.
 *
 * @param stream  File pointer representing the stream from which to read.
 * @param buf     Caller-allocated buffer in which to store the line.
 * @return        On success, the length of the string read, in bytes.
 *                Or 0 on EOF, <0 on error.
 */
int read_line(FILE *stream, char *buf) {
  // Read from stream and handle errors
  if(!fgets(buf, MAX_LINE, stream)) {
    if(feof(stream)) {
      // Handle EOF (also print newline)
      putchar('\n');
      return 0;
    } else {
      // Handle actual spooky errors
      perror(ERR_MSG);
      return -1;
    }
  }

  return strlen(buf);
}

/**
 * The shell's read-evaluate-print cycle. Reads a line of input from a stream,
 * parses and evaluates it, and prints the result.
 *
 * @param buf      Buffer containing the line from the input stream to act on.
 * @param buf_len  Length of the line buffer.
 * @return         True if execution should continue after this iteration,
 *                 false if we've reached the end of the input.
 */
bool shell_repl(char *buf, size_t buf_len) {
  fflush(stdout); // Flush output from last run before starting new run

  strip_newline(buf, buf_len);

  if (!strcmp(buf, "quit") || !strcmp(buf, "exit")) {
    // "quit" handling required; "exit" just becuase I keep forgetting
    return false;
  } else if(buf_len) { // Only exec if entry is not just "\n"
    int myargc = 0;
    char *myargv[MAX_LINE/2 + 1] = {NULL};

    parse_input(buf, &myargc, myargv);
    execute(myargv, strip_bg(&myargc, myargv));

    free_args(myargv);
  }

  return true;
}
