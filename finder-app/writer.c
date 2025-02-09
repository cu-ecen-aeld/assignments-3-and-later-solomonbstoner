#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

int main (int argc, char **argv)
{
	openlog("WRITER ", LOG_PID, LOG_USER);

	if (argc != 3)
	{
		syslog(LOG_USER | LOG_ERR, "Insufficient arguments!");
		closelog();
		exit(1);
	}
	char *writefile = argv[1];
	char *writestr = argv[2];

	FILE *file = NULL;

	file = fopen(writefile,"w");
	if (file == NULL)
	{
		syslog(LOG_USER | LOG_ERR, "Can't open file because: %s", strerror(errno));
		closelog();
		exit(1);
	}
	size_t cw = fwrite(writestr, 1, strlen(writestr), file); // fwrite doesnt set errno
	if (fclose(file) != 0)
	{
		syslog(LOG_USER | LOG_ERR, "Can't close file because: %s", strerror(errno));
		closelog();
		exit(1);
	}
	syslog(LOG_USER | LOG_INFO, "Wrote %ld chars to %s", cw, writefile);
	exit(0);
}
