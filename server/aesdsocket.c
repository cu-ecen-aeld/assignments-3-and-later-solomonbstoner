#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT_NUM "9000"

volatile bool to_continue = true;

void term_sig_handl (int signum)
{
	// keep as simple as possible as it has to be reentrant
}

int main ()
{
	struct addrinfo *skaddr_ptr; // initialized by getaddrinfo
	struct addrinfo inc_sock; // information of incoming connecting socket

	int ret = 0; // placeholder for function return values
	openlog("aesdsocket", LOG_PID, LOG_USER); // Initialize syslog

	// Open a stream socket bound to port 9000. Return -1 if any connection steps fail
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1)
	{
		syslog(LOG_USER | LOG_ERR, "Failure to initialize socket");
		exit(-1);
	}

	ret = getaddrinfo("localhost", PORT_NUM, NULL, &skaddr_ptr);
	if (ret != 0)
	{
		syslog(LOG_USER | LOG_ERR, "Failure to getaddrinfo: %s", gai_strerror(ret));
		exit(-1);
	}

	ret = bind(sfd, skaddr_ptr->ai_addr, sizeof(struct sockaddr));
	if (ret == -1)
	{
		syslog(LOG_USER | LOG_ERR, "Failure to bind socket");
		exit(-1);
	}

	ret = listen(sfd, 1); // Mark socket as a listening socket
	if (ret == -1)
	{
		syslog(LOG_USER | LOG_ERR, "Failure to listen");
		exit(-1);
	}

	// Fork as a daemon when the '-d' argument is given
	bool is_daemon = false;
	while ((c = getopt(argc, argv, "d::")) != -1)
	{
		switch (c)
		{
		case 'd':
			is_daemon = true;
			break;
		default:
	}

	if (is_daemon)
	{
		// Fork as a new process and quit this foreground process
		pid_t p = fork();
		if (p == -1)
		{
			// Error
			exit(-1);
		}
		else if (p != 0)
		{
			// Parent
			exit(0);
		}
		// Child process continues to the while loop
	}

	// Continuously listen for conn until SIGINT / SIGTERM is received. Then log "Caught signal, exiting" and "Closed connection from X.X.X.X" when SIGINT / SIGTERM is received
	while (to_continue)
	{
		int cfd = accept(sfd, inc_sock, sizeof(struct sockaddr));
		if (cfd == -1)
		{
			// Error connecting
		}
		// TODO: New thread for each connection?
		syslog(LOG_USER | LOG_INFO, "Accepted connection from %s", inet_ntoa(inc_sock->sin_addr)); // Logs msg to syslog "Accepted connection from x.x.x.x"
		

		// Receive data from the conn, and append it to file `/var/tmp/aesdsocketdata`. Discard over-length packets (ie if malloc fails)

		// Return the FULL content of `/var/tmp/aesdsocketdata` to the client as soon as a new packet is received (delimited by '\n')

	}

	syslog(LOG_USER | LOG_NOTICE, "Caught signal, exiting");

	// Close open sockets
}
