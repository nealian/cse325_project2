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
#include <stdio.h> // printf, fflush, fgets, fopen
#include <stdlib.h> // exit, free, malloc
#include <string.h> // strncpy, strlen, strtok, strcmp
#include <sys/wait.h> // waitpid, WNOHANG
#include <unistd.h> // fork, execvp
#include <stdbool.h> // bool, true, false
#include <ctype.h> // isspace

// *CONSTANTS*
#define ARG_DELIMITER " " // The delimiter between arguments
#define CUR_DELIMITER ";" // The delimiter between concurrent statements
#define MAX_LINE 1024 // The maximum number of characters to allow for input
#define PS1 "SANIC TEEM> " // The default prompt; $PS1 is the prompt variable
#define ERR_MSG "shell: Error" // Error message, for perror

// *FUNCTION PROTOTPYES*
int execute_many(char *args[MAX_LINE/2][MAX_LINE/2 + 1], size_t num);
void free_args(char **args);
char *mystrcpy(char *src, size_t size);
void parse_input(char *buf, char **argv);
void strip_newline(char *str, size_t len);
int run_interactive();
int run_batch(char *fname);
int read_line(FILE *stream, char *buf);
size_t trimwhitespace(char *out, size_t len, const char *str);
bool split_concurrent(char *buf, size_t buf_len, char *args[MAX_LINE/2][MAX_LINE/2 + 1], int *num);
bool shell_repl(char *buf, size_t buf_len);

// *MAIN PROGRAM*
int main(int argc, char **argv) {

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
 * Execute the concurrent commands.  Will wait to return until all children have
 * finished execution.  Referenced http://stackoverflow.com/a/1381144
 *
 * @param args  Array of arrays of strings.  For each array of strings, the
 *              first element is the program to run, the rest are arguments
 * @param num   The number of concurrent commands to run; we here assume that
 *              args[] is of this length.
 * @return      The status of the last command; complete status field from
 *              waitpid(2) (to get exit status, use WIFEXITED and WEXITSTATUS)
 */
int execute_many(char *args[MAX_LINE/2][MAX_LINE/2 + 1], size_t num) {
  pid_t pid;
  pid_t *children = calloc(num, sizeof(pid_t));
  int *child_status = calloc(num, sizeof(int));
  int ret_status;
  int i;
  bool waiting;

  // Child execution
  for (i=0; i < num; i++) {
    pid = fork();
    if (pid == 0) {
      // Am child; do child-y things
      if (execvp(args[i][0], args[i]) < 0) { // An error occured exec()ing
        perror(ERR_MSG);
      }
      exit(0);
    } else if (pid < 0) { // An error occured fork()ing
      perror(ERR_MSG);
    } else {
      children[i] = pid;
    }
  }
  
  // Waiting for children
  do {
    waiting = false;
    for (i=0; i<num; i++) {
      if (children[i] > 0) {
        pid = waitpid(children[i], &(child_status[i]), WNOHANG);
        // use WNOHANG for concurrency
        if (pid < 0) { // An error occured
          perror(ERR_MSG);
        } else if (pid == 0) { // Still waiting on children[i]
          waiting = true;
        } else { // This particular child is done!
          children[i] = 0;
        }
      }
      // Let everyting run
      sleep(1);
    }
  } while (waiting);

  // We malloc()d it, now we gotta free() it.
  free(children);
  for (i=0; i<num; i++)
    free_args(args[i]);

  ret_status = child_status[num - 1]; // Return the last status
  free(child_status);
  return ret_status;
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
 * Transfer the ARG_DELIMITER-separated string (buf) to separate entries in args
 *
 * @param buf   A line taken from input with a command and list of arguments.
 * @param argv  Empty array of strings. On return, contains a representation of
 *              the command as a list of arguments. Each element must be freed
 *              by the caller.
 */
void parse_input(char *buf, char **argv) {
  int argc = 0;
  char *bufcpy = NULL;
  char *token = NULL;
  bufcpy = mystrcpy(buf, MAX_LINE); // strtok() modifies the original
  token = strtok(bufcpy, ARG_DELIMITER);
  if (strlen(token)) {
    argv[argc] = mystrcpy(token, strlen(token) + 1); // Needs room for the NULL
  } else {
    argc--;
  }
  for(argc++; (token = strtok(NULL, ARG_DELIMITER)); argc++) {
    if (strlen(token)) { // Check for extra ARG_DELIMITERs
      argv[argc] = mystrcpy(token, strlen(token) + 1);
    } else {
      // We have an empty string (there was an extra ARG_DELIMITER here)
      argc--;
    }
  }
  free(bufcpy);
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

  FILE *fstream;
  if(!(fstream = fopen(fname, "r"))) {
    perror(ERR_MSG);
    printf("Could not open batch file %s, aborting...\n", fname);
    return -1;
  }

  char buf[MAX_LINE]; // Line buffer
  int buf_len;
  do {
    if((buf_len = read_line(fstream, buf)) <= 0) {
      fclose(fstream);
      return buf_len;
    }

    // Print input line
    printf(buf);
  } while(shell_repl(buf, buf_len - 1));

  fclose(fstream);
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
 * Trim; that is, strip leading and trailing whitespace, from the buffer, and
 * store in the given output buffer (which must be big enough to store the
 * result).  From http://stackoverflow.com/a/122721/1342300
 *
 * @param out   The output buffer, already allocated
 * @param len   The length of the input buffer
 * @param str   The input buffer
 *
 * @return      The length of the new output buffer
 */
size_t trimwhitespace(char *out, size_t len, const char *str)
{
  if(len == 0)
    return 0;

  const char *end;
  size_t out_size;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
  {
    *out = 0;
    return 1;
  }

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;
  end++;

  // Set output size to minimum of trimmed string length and buffer size minus 1
  out_size = (end - str) < len-1 ? (end - str) : len-1;

  // Copy trimmed string and add null terminator
  memcpy(out, str, out_size);
  out[out_size] = 0;

  return out_size;
}

/**
 * Split the current buffer into an array of commands to be run.
 *
 * @param buf     The buffer of the current line of input; will be modified.
 * @param buf_len The length of the buffered line
 * @param args    A reference to the array we'll allocate and fill here
 * @param num     A reference to the number of commands
 * @return        True if execution should continue after this iteration,
 *                false if we've reached the end of the input.
 */
bool split_concurrent(char *buf, size_t buf_len, char *args[MAX_LINE/2][MAX_LINE/2 + 1], int *num) {
  *num = 0;
  int i=0;
  char *bufcpy = NULL;
  char *token = NULL;
  char *subbufcpy = NULL;
  char *command_buffers[MAX_LINE/2] = {NULL};
  bool to_continue = true;
  bufcpy = mystrcpy(buf, buf_len+1);
  token = strtok(bufcpy, CUR_DELIMITER);
  subbufcpy = calloc(strlen(token) + 1, sizeof(char *));
  if (trimwhitespace(subbufcpy, strlen(token) + 1, token)) {
    // ^Trim input and check for actual statement
    if (!strcmp(subbufcpy, "quit") || !strcmp(subbufcpy, "exit")) {
      // "quit" handling required; "exit" just because I keep forgetting
      to_continue = false;
      (*num)--; // Don't add an element for this
      free(subbufcpy);
    } else {
      command_buffers[*num] = subbufcpy;
    }
  } else {
    (*num)--; // Don't add an element for this; it's empty
  }

  for ((*num)++; (token = strtok(NULL, CUR_DELIMITER)); (*num)++) {
    subbufcpy = calloc(strlen(token) + 1, sizeof(char *));
    if (trimwhitespace(subbufcpy, strlen(token) + 1, token)) {
      // ^Trim input and check for actual statement
      if (!strcmp(subbufcpy, "quit") || !strcmp(subbufcpy, "exit")) {
        // "quit" handling required; "exit" just because I keep forgetting
        to_continue = false;
        (*num)--; // Don't add an element for this
        free(subbufcpy);
      } else {
        command_buffers[*num] = subbufcpy;
      }
    } else {
      (*num)--; // Don't add an element for this; it's empty
    }
  }

  for(i=0; i<*num; i++) {
    // Now actually process them; separated because strtok() is stateful
    parse_input(command_buffers[i], args[i]);
    free(command_buffers[i]);
  }
  free(bufcpy);
  return to_continue;
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

  char *args[MAX_LINE/2][MAX_LINE/2 + 1];
  int num_concurrent;
  bool to_continue = true;

  to_continue = split_concurrent(buf, buf_len, args, &num_concurrent);
  execute_many(args, num_concurrent);

  return to_continue;
}
