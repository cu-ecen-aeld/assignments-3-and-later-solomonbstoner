#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <syslog.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#define PORT_NUM "9000"
#define FILE_NAME "/var/tmp/aesdsocketdata"

void term_sig_handl (int signum)
{
	// keep as simple as possible as it has to be reentrant
	unlink(FILE_NAME); // delete the file
	syslog(LOG_USER | LOG_NOTICE, "Caught signal, exiting"); // TODO: is this signal safe?
	exit(0);
}

int main (int argc, char **argv)
{
	struct addrinfo *skaddr_ptr; // initialized by getaddrinfo
	struct sockaddr_in inc_sock; // information of incoming connecting socket

	int ret = 0; // placeholder for function return values
	openlog("aesdsocket", LOG_PID, LOG_USER); // Initialize syslog
	signal(SIGTERM, term_sig_handl);
	signal(SIGINT, term_sig_handl);

	// Open a stream socket bound to port 9000. Return -1 if any connection steps fail
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	const int enable = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)); // SO_REUSEADDR to get rid of bind error
	// TODO: what if setsockopt fails?
	if (sfd == -1)
	{
		syslog(LOG_USER | LOG_ERR, "Failure to initialize socket");
		exit(-1);
	}

	ret = getaddrinfo("0.0.0.0", PORT_NUM, NULL, &skaddr_ptr);
	if (ret != 0)
	{
		freeaddrinfo(skaddr_ptr);
		syslog(LOG_USER | LOG_ERR, "Failure to getaddrinfo: %s", gai_strerror(ret));
		exit(-1);
	}

	ret = bind(sfd, skaddr_ptr->ai_addr, sizeof(struct sockaddr));
	if (ret == -1)
	{
		freeaddrinfo(skaddr_ptr);
		syslog(LOG_USER | LOG_ERR, "Failure to bind socket");
		exit(-1);
	}

	ret = listen(sfd, 1); // Mark socket as a listening socket
	if (ret == -1)
	{
		freeaddrinfo(skaddr_ptr);
		syslog(LOG_USER | LOG_ERR, "Failure to listen");
		exit(-1);
	}

	// Fork as a daemon when the '-d' argument is given
	bool is_daemon = false;
	char c;
 	while ((c = getopt(argc, argv, "d::")) != (char) -1) // infinite loop if no char cast is there
 	{
 		switch (c)
 		{
 		case 'd':
 			is_daemon = true;
 			break;
 		default:
 			break;
 		}
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

	syslog(LOG_USER | LOG_INFO, "Setup successful");

	// Continuously listen for conn until SIGINT / SIGTERM is received. Then log "Caught signal, exiting" and "Closed connection from X.X.X.X" when SIGINT / SIGTERM is received
	int cfd = -1;
	while (true)
	{
		int fd = open(FILE_NAME, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd == -1)
		{
			freeaddrinfo(skaddr_ptr);
			syslog(LOG_USER | LOG_ERR, "Failure to open file %s. Error: %s", FILE_NAME, strerror(errno));
			exit(1);
		}
		socklen_t inc_sock_size = sizeof(struct sockaddr);
		cfd = accept(sfd, (struct sockaddr *)&inc_sock, &inc_sock_size);
		if (cfd < 0)
		{
			// Error connecting
			syslog(LOG_USER | LOG_ERR, "Failure to accept connection: %s", strerror(errno));
			close(fd);
			continue;
		}
		// TODO: New thread for each connection?
		syslog(LOG_USER | LOG_INFO, "Accepted connection from %s", inet_ntoa(inc_sock.sin_addr));
		

		// Receive data from the conn, and append it to file `/var/tmp/aesdsocketdata`. Discard over-length packets (ie if malloc fails)
		while (recv(cfd, &c, 1, 0) == 1)
		{
			write(fd, &c, 1); // TODO: Handle failure to write?
			if (c == '\n')
				break; // end of packet received
		}

		// Return the FULL content of `/var/tmp/aesdsocketdata` to the client as soon as a new packet is received (delimited by '\n')
		lseek(fd, SEEK_SET, 0); // start at the front of the file
		while(read(fd, &c, 1) == 1)
		{
			send(cfd, &c, 1, 0);
		}
		syslog(LOG_USER | LOG_INFO, "Closed connection from %s", inet_ntoa(inc_sock.sin_addr));
		close(fd);
		close(cfd);
		cfd = -1;
	}
	

}
