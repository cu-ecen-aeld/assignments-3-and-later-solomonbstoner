#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO:
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
	int ret = system(cmd);
	if (ret == -1) return false; // child process could not be created
	if (ret == 127) return false; // shell could not be executed in the child process
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

	pid_t p = fork();
	if (p == -1) return false;
	if (p == 0)
	{
		// Child
		execv(command[0], command);
		exit(255); // Never reached unless execv returns; which only happens when an error occurs. Child to exit with status 255
	}

	// Parent
	int stat_loc = 0;
	wait(&stat_loc);
    va_end(args);
	if (WIFEXITED(stat_loc) == true && WEXITSTATUS(stat_loc) == 0)
		return true; // Child process exited normally and with status zero
	else
    	return false; // Child process exited with non-zero
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

	pid_t p = fork();
	if (p == -1) return false;
	if (p == 0)
	{
		// Child
		int fd = open(outputfile, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // rw- r-- r--
		// We want to overwrite existing files, and create a new file if it doesn't exist
		if (fd == -1)
			exit(1); // Failed to open file
		if (dup2(fd, 1) < 0)
		{
			close(fd);
			exit(1); // Failed to replace stdout with outputfile in the child process
		}
		execv(command[0], command);
		exit(1); // Never reached unless execv returns; which only happens when an error occurs. Child to exit with status 1
	}

	// Parent
	int stat_loc = 0;
	wait(&stat_loc);
    va_end(args);
	if (WIFEXITED(stat_loc) == true && WEXITSTATUS(stat_loc) == 0)
		return true; // Child process exited with zero
	else
    	return false; // Child process exited with non-zero
}
