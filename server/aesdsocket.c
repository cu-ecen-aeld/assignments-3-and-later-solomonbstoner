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
#include <sys/queue.h> // For the linked list
#include <pthread.h>

#define PORT_NUM "9000"
#define FILE_NAME "/var/tmp/aesdsocketdata"

// struct that contains the pthread_t info, socket id, mutex, bool to indicate if the thread is complete, and pointer to itself. This is the node of the linked list
struct node
{
	pthread_mutex_t ll_m; // mutex for this element
	pthread_t t_id; // thread id of this particular thread
	char ip_a[INET_ADDRSTRLEN]; // store the IPv4 addr of the connected client in string representation
	int sock_fd; // client socket file descriptor
	bool is_completed; // true when the thread is ready to be terminated
	SLIST_ENTRY(node) next; // macro for the next element in the linked list
};

SLIST_HEAD(slisthead, node); // head of the singly linked list
struct slisthead head;

int fd;
pthread_mutex_t fd_m; // mutex for FILE_NAME fd

void *thread_func (void *); // declaration of the func that the thread will run

void term_sig_handl (int signum)
{
	// keep as simple as possible as it has to be reentrant
	unlink(FILE_NAME); // delete the file
	syslog(LOG_USER | LOG_NOTICE, "Caught signal, exiting"); // TODO: is this signal safe?
	exit(0); // Let the OS clean up everything for us
	// TODO: "after requesting an exit from each thread and waiting for threads to complete execution."
	// TODO: something about pthread_kill
}

int main (int argc, char **argv)
{
	struct addrinfo *skaddr_ptr; // initialized by getaddrinfo
	struct sockaddr_in inc_sock; // information of incoming connecting socket

	int ret = 0; // placeholder for function return values
	openlog("aesdsocket", LOG_PID, LOG_USER); // Initialize syslog
	signal(SIGTERM, term_sig_handl);
	signal(SIGINT, term_sig_handl);

	SLIST_INIT(&head); // Initialize the head of the linked list

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
	fd = open(FILE_NAME, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	pthread_mutex_init(&fd_m, NULL);
	if (fd == -1)
	{
		freeaddrinfo(skaddr_ptr);
		syslog(LOG_USER | LOG_ERR, "Failure to open file %s. Error: %s", FILE_NAME, strerror(errno));
		exit(1);
	}

	syslog(LOG_USER | LOG_INFO, "Setup successful");

	// Continuously listen for conn until SIGINT / SIGTERM is received. Then log "Caught signal, exiting" and "Closed connection from X.X.X.X" when SIGINT / SIGTERM is received
	int cfd = -1;
	while (true)
	{
		socklen_t inc_sock_size = sizeof(struct sockaddr);
		cfd = accept(sfd, (struct sockaddr *)&inc_sock, &inc_sock_size);
		if (cfd < 0)
		{
			// Error connecting
			syslog(LOG_USER | LOG_ERR, "Failure to accept connection: %s", strerror(errno));
			continue;
		}

		struct node *n_new = (struct node *) malloc(sizeof(struct node)); // malloc a new struct to append to the end of the linked list
		pthread_mutex_init(&n_new->ll_m, NULL); // init the mutex with default attr
		pthread_mutex_lock(&n_new->ll_m); // mutex will be unlocked after the node is appended to the linked list
		n_new->sock_fd = cfd;
		inet_ntop(AF_INET, &(inc_sock.sin_addr), n_new->ip_a, INET_ADDRSTRLEN);
		syslog(LOG_USER | LOG_INFO, "Accepted connection from %s", inet_ntoa(inc_sock.sin_addr));
		struct node *n_tmp = NULL;
		// TODO: What happens if malloc fails?
		SLIST_FOREACH(n_tmp, &head, next) // Iterate through the struct and pthread_join all completed threads
		{
			pthread_mutex_lock(&n_tmp->ll_m);
			if (n_tmp->is_completed == false)
			{
				pthread_mutex_unlock(&n_tmp->ll_m); // nothing to do. unlock mutex and move on
				continue;
			}
			pthread_join(n_tmp->t_id, NULL); // join thread
			// TODO: what if pthread_join fails?
			SLIST_REMOVE(&head, n_tmp, node, next); // remove node n_tmp
			pthread_mutex_unlock(&n_tmp->ll_m);
			pthread_mutex_destroy(&n_tmp->ll_m); // destroying a locked mutex results in undefined behavior
			free(n_tmp); // free node
		}
		// After the loop ends, n_tmp should point to the end of the list
		pthread_create(&n_new->t_id, NULL, thread_func, (void *) n_new); // New default thread for the new connection
		if (n_tmp == NULL)
			SLIST_INSERT_HEAD(&head, n_new, next); // First entry
		else
			SLIST_INSERT_AFTER(n_tmp, n_new, next); // Not first entry
		pthread_mutex_unlock(&n_new->ll_m); // unlock mutex for the new thread to do its work
	}
	close(fd); // close the file
}

void *thread_func (void *arg) 
{
	struct node *n = (struct node *) arg; // to shut the compiler up about incompatible arg type
	// n: addr to the linked list node corresponding to this thread
	pthread_mutex_lock(&n->ll_m); // Lock node mutex
	char c;
	// Receive data from the conn, and append it to file `/var/tmp/aesdsocketdata`. Discard over-length packets (ie if malloc fails)

	pthread_mutex_lock(&fd_m);
	while (recv(n->sock_fd, &c, 1, 0) == 1)
	{
		write(fd, &c, 1); // ignore failure to write
		if (c == '\n')
			break; // end of packet received
	}
	pthread_mutex_unlock(&fd_m);

	pthread_mutex_lock(&fd_m);
	lseek(fd, SEEK_SET, 0); // start at the front of the file
	while(read(fd, &c, 1) == 1)
	{
		// Return the FULL content of `/var/tmp/aesdsocketdata` to the client as soon as a new packet is received (delimited by '\n')
		send(n->sock_fd, &c, 1, 0);
	}
	pthread_mutex_unlock(&fd_m);

	syslog(LOG_USER | LOG_INFO, "Closed connection from %s", n->ip_a);

	close(n->sock_fd);
	n->is_completed = true;
	n->sock_fd = -1;
	pthread_mutex_unlock(&n->ll_m);
	return NULL; // to shut the compiler up about void*
}
