#include <stdio.h>

int main ()
{
	// Open a stream socket bound to port 9000. Return -1 if any connection steps fail

	// Fork as a daemon when the '-d' argument is given

	// Continuously listen for conn until SIGINT / SIGTERM is received. Then log "Caught signal, exiting" and "Closed connection from X.X.X.X" when SIGINT / SIGTERM is received

	// Logs msg to syslog "Accepted connection from x.x.x.x"

	// Receive data from the conn, and append it to file `/var/tmp/aesdsocketdata`. Discard over-length packets (ie if malloc fails)

	// Return the FULL content of `/var/tmp/aesdsocketdata` to the client as soon as a new packet is received (delimited by '\n')
}
