#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h> // For system
#include <sys/wait.h> // For wait
#include <unistd.h> // For execv, dup2
#include <fcntl.h> // For open

bool do_system(const char *command);

bool do_exec(int count, ...);

bool do_exec_redirect(const char *outputfile, int count, ...);
