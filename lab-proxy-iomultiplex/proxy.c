#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXEVENTS 64
#define MAXLINE 2048
#define READ_REQUEST 0
#define SEND_REQUEST 1
#define READ_RESPONSE 2
#define SEND_RESPONSE 3
#define FINISHED 4

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
int open_sfd(char *port);
void handle_new_clients();
void print_bytes(unsigned char *, int);
void handle_client();

struct request_info
{
	int mysocketclient;
	int mysocketweb;
	int state;
	char buf_read[MAX_CACHE_SIZE];
	char buf_write[MAX_CACHE_SIZE];
	int totalBytesReadFromClient;
	int totalBytesToWriteToServer;
	int totalBytesWrittenToServer;
	int totalBytesReadFromServer;
	int totalByetsWrittenToClient;
};

struct request_info *new_request;
struct request_info *active_request;
struct epoll_event event;
struct epoll_event *events;

int global_flag = 0;

void sig_handler(int signum)
{
	global_flag = 1;
}

int main(int argc, char *argv[])
{
	// test_parser();
	// printf("%s\n", user_agent_hdr);

	struct sigaction sigact;

	sigact.sa_flags = SA_RESTART; // zero out flags
	sigact.sa_handler = sig_handler;
	sigaction(SIGINT, &sigact, NULL);

	int erfd, ewfd;
	int i;

	struct epoll_event event;
	struct epoll_event *events;

	if ((erfd = epoll_create1(0)) < 0) // reading
	{
		fprintf(stderr, "error creating epoll fd\n");
		exit(1);
	}

	if ((ewfd = epoll_create1(0)) < 0) // writing
	{
		fprintf(stderr, "error creating epoll fd\n");
		exit(1);
	}

	int sfd = open_sfd(argv[1]);

	new_request = malloc(sizeof(struct request_info));
	new_request->mysocketclient = sfd;
	// sprintf(listener->desc, "Listen file descriptor (accepts new clients)");

	// register the listening file descriptor for incoming events using
	// edge-triggered monitoring
	event.data.ptr = new_request;
	event.events = EPOLLIN | EPOLLET;

	if (epoll_ctl(erfd, EPOLL_CTL_ADD, sfd, &event) < 0)
	{
		fprintf(stderr, "error adding event\n");
		exit(1);
	}

	/* Buffer where events are returned */
	events = calloc(MAXEVENTS, sizeof(struct epoll_event));

	while (1)
	{
		// wait for event to happen (-1 == no timeout)
		int n = epoll_wait(erfd, events, MAXEVENTS, 1000);

		if (n == 0)
		{
			if (global_flag)
			{
				break;
			}
		}
		else if (n < 0)
		{
			if (errno == EBADF)
			{
				fprintf(stderr, "epfd is not a valid file descriptor.\n");
				break;
			}
			else if (errno == EFAULT)
			{
				fprintf(stderr, "The memory area pointed to by events is not accessible with write permissions.\n");
				break;
			}
			else if (errno == EINTR)
			{
				fprintf(stderr, "EINTR error\n");
				break;
			}
			else if (errno == EINVAL)
			{
				fprintf(stderr, "EINVAL error\n");
				break;
			}
		}

		for (i = 0; i < n; ++i)
		{
			// grab the data structure from the event, and cast it
			// (appropriately) to a struct client_info *.
			active_request = (struct request_info *)(events[i].data.ptr); // Existig client

			// printf("New event for %s\n", active_client->desc);

			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				(events[i].events & EPOLLRDHUP))
			{
				/* An error has occured on this fd */
				fprintf(stderr, "epoll error on %d\n", active_request->mysocketclient);
				close(active_request->mysocketclient);
				close(active_request->mysocketweb);
				free(active_request);
				continue;
			}

			if (active_request->mysocketclient == sfd)
			{
				// printf("Handle new Clients\n");
				handle_new_clients(erfd, sfd);
			}
			else
			{
				// read from socket until (1) the remote side
				// has closed the connection or (2) there is no
				// data left to be read.
				// printf("Handle client\n");
				handle_client(erfd, ewfd, active_request);
			}
		}
	}

	free(events);
	// free(listener);
	close(erfd);
	close(ewfd);
	close(sfd);

	return 0;
}

int open_sfd(char *port)
{

	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));

	/* As a server, we want to exercise control over which protocol (IPv4
	   or IPv6) is being used, so we specify AF_INET or AF_INET6 explicitly
	   in hints, depending on what is passed on on the command line. */
	hints.ai_family = AF_INET;		 /* Choose IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;	 /* For wildcard IP address */
	hints.ai_protocol = 0;			 /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	s = getaddrinfo(NULL, port, &hints, &result);
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sfd = socket(rp->ai_family, rp->ai_socktype,
					 rp->ai_protocol);
		if (sfd == -1)
		{
			continue;
		}
		break;
	}

	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}

	if (bind(sfd, result->ai_addr, result->ai_addrlen) < 0)
	{
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	// freeaddrinfo(result); /* No longer needed */

	listen(sfd, 100);

	if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}

	return sfd;
}

int all_headers_received(char *request)
{
	if (strstr(request, "\r\n\r\n") != NULL)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int parse_request(char *request, char *method,
				  char *hostname, char *port, char *path, char *headers)
{

	if (!all_headers_received(request))
	{
		return 0;
	}

	char *reqs;
	int exists = 0;
	unsigned int i = 0;

	// Start Request
	while (i < strlen(request))
	{
		if (request[i] == ' ')
		{
			exists = 1;
			break;
		}
		++i;
	}

	if (exists != 1)
	{
		return 0;
	}

	// Method GET
	strncpy(method, request, i);
	method[i] = '\0';

	// Add to request
	reqs = strstr(request, "//");
	i = 2;
	int defaultPort = 0;
	exists = 0;

	// Which port to use
	while (i < strlen(reqs)) // locate needs default port
	{
		if (reqs[i] == '/')
		{
			exists = 1;
			defaultPort = 1;
			break;
		}
		else if (reqs[i] == ':')
		{
			exists = 1;
			defaultPort = 0;
			break;
		}
		++i;
	}

	if (exists != 1)
	{
		return 0;
	}

	// HostName
	strncpy(hostname, &reqs[2], i - 2);

	// Maybe not needed, try it out
	hostname[i - 2] = '\0';

	if (defaultPort == 1)
	{
		strcpy(port, "80"); // default port
		reqs = &reqs[i];
	}
	else
	{
		exists = 0;
		reqs = strchr(reqs, ':'); // add the correct port to buf
		i = 1;
		while (i < strlen(reqs))
		{
			if (reqs[i] == '/')
			{
				exists = 1;
				break;
			}
			++i;
		}
		if (exists != 1)
		{
			return 0;
		}
		strncpy(port, &reqs[1], i - 1);
		port[i - 1] = '\0';
		reqs = &reqs[i];
	}

	exists = 0;
	i = 0;

	while (i < strlen(reqs))
	{
		if (reqs[i] == ' ')
		{
			exists = 1;
			break;
		}
		++i;
	}

	// PathName
	strncpy(path, &reqs[0], i);
	path[i] = '\0';

	// Final Headers
	reqs = strstr(request, "\r\n");
	strcpy(headers, &reqs[2]);

	// Finished?

	return 1;
}

void handle_new_clients(int efd, int fd)
{

	// loop until all pending clients have been accepted
	while (1)
	{
		struct sockaddr_storage clientaddr;
		// struct client_info *new_client;
		socklen_t clientlen = sizeof(struct sockaddr_storage);

		int connfd = accept(fd, (struct sockaddr *)&clientaddr, &clientlen);

		if (connfd < 0)
		{
			if (errno == EWOULDBLOCK ||
				errno == EAGAIN)
			{
				// no more clients ready to accept
				break;
			}
			else
			{
				fprintf(stderr, "accept");
				exit(EXIT_FAILURE);
			}
		}

		// set client file descriptor non-blocking
		if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK) < 0)
		{
			fprintf(stderr, "error setting socket option\n");
			exit(1);
		}

		// allocate memory for a new struct
		// client_info, and populate it with
		// info for the new client
		new_request = (struct request_info *)malloc(sizeof(struct request_info));
		new_request->mysocketclient = connfd;
		fprintf(stderr, "my new fd: %d\n", new_request->mysocketclient);
		// sprintf(new_client->desc, "Client with file descriptor %d", connfd);

		// Setting initial state here?
		// request->state = READ_REQUEST;

		// register the client file descriptor
		// for incoming events using
		// edge-triggered monitoring
		event.data.ptr = new_request;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event) < 0)
		{
			fprintf(stderr, "error adding event\n");
			exit(1);
		}
	}
}

void handle_client(int erfd, int ewfd, struct request_info *request)
{
	// printf("I'm in handle_client\n");
	// printf("I'm the fd = %d", client_request->mysocketclient);

	if (request->state == READ_REQUEST)
	{
		// read me
		// printf("Initiating Read Request\n");

		while (1)
		{
			int len = recv(request->mysocketclient, &request->buf_read[request->totalBytesReadFromClient], MAXLINE, 0);

			if (len < 0)
			{
				if (errno == EWOULDBLOCK ||
					errno == EAGAIN)
				{
					// no more data to be read
				}
				else
				{
					perror("client recv");
					close(request->mysocketclient);
					free(request);
				}
				return;
			}

			request->totalBytesReadFromClient += len;

			if (all_headers_received(request->buf_read) == 1)
			{
				break;
			}
		}

		char method[16], hostname[64], port[8], path[64], headers[1024];

		// printf("Here my buf %s\n", buf);

		if (parse_request(request->buf_read, method, hostname, port, path, headers))
		{
			if (strcmp(port, "80"))
			{
				sprintf(request->buf_write, "%s %s HTTP/1.0\r\nHost: %s:%s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
						method, path, hostname, port, user_agent_hdr);
			}
			else
			{
				sprintf(request->buf_write, "%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n",
						method, path, hostname, user_agent_hdr);
			}
			request->totalBytesToWriteToServer = strlen(request->buf_write);
			printf("%s", request->buf_write);
		}
		else
		{
			printf("REQUEST INCOMPLETE\n");
			close(request->mysocketclient);
			free(request);
			return;
		}

		int sfd, s;
		struct addrinfo hints, *results, *rp;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		hints.ai_protocol = 0; /* Any protocol */

		if ((s = getaddrinfo(hostname, port, &hints, &results)) != 0)
		{
			fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(s));
			close(request->mysocketclient);
			free(request);
			return;
		}

		for (rp = results; rp != NULL; rp = rp->ai_next)
		{
			sfd = socket(rp->ai_family, rp->ai_socktype,
						 rp->ai_protocol);

			if (sfd == -1)
				continue;

			if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
				break; /* Success */

			close(sfd);
		}

		// Configure socket as non-blocking

		if (fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK) < 0)
		{
			fprintf(stderr, "error setting socket option\n");
			close(request->mysocketclient);
			free(request);
			exit(1);
		}

		// Register the socket with the epoll instance for writing

		request->mysocketweb = sfd;
		event.data.ptr = request;
		event.events = EPOLLOUT | EPOLLET;

		if (epoll_ctl(ewfd, EPOLL_CTL_ADD, sfd, &event) < 0)
		{
			close(request->mysocketclient);
			close(request->mysocketweb);
			free(request);
			return;
		}

		// Change state to SEND_REQUEST

		memset(request->buf_read, '\0', request->totalBytesReadFromClient);
		request->state = SEND_REQUEST;
	}

	if (request->state == SEND_REQUEST)
	{
		// printf("Initiating Send Request\n");

		while (1)
		{
			int len = write(request->mysocketweb, &request->buf_write[request->totalBytesWrittenToServer], request->totalBytesToWriteToServer);

			if (len < 0)
			{
				if (errno == EWOULDBLOCK ||
					errno == EAGAIN)
				{
					// no more data to be read
				}
				else
				{
					perror("server write");
					close(request->mysocketclient);
					close(request->mysocketweb);
					free(request);
				}
				return;
			}

			request->totalBytesWrittenToServer += len;

			if (request->totalBytesWrittenToServer >= request->totalBytesToWriteToServer)
			{
				break;
			}
		}

		event.data.ptr = request;
		event.events = EPOLLIN | EPOLLET;

		if (epoll_ctl(erfd, EPOLL_CTL_ADD, request->mysocketweb, &event) < 0)
		{
			perror("error adding event\n");
			exit(1);
		}

		memset(request->buf_write, '\0', request->totalBytesWrittenToServer);

		request->state = READ_RESPONSE;
	}
	if (request->state == READ_RESPONSE)
	{
		// printf("Initiating Read Response\n");

		while (1)
		{
			int len = recv(request->mysocketweb, &request->buf_read[request->totalBytesReadFromServer], MAXLINE, 0);

			if (len < 0)
			{
				if (errno == EWOULDBLOCK ||
					errno == EAGAIN)
				{
					// no more data to be read
				}
				else
				{
					perror("read response");
					close(request->mysocketclient);
					free(request);
				}
				return;
			}

			if (len == 0)
			{
				break;
			}

			request->totalBytesReadFromServer += len;
		}

		event.data.ptr = request;
		event.events = EPOLLOUT | EPOLLET;

		if (epoll_ctl(ewfd, EPOLL_CTL_ADD, request->mysocketclient, &event) < 0)
		{
			fprintf(stderr, "error adding event\n");
			exit(1);
		}

		request->totalBytesToWriteToServer = request->totalBytesReadFromServer;

		request->state = SEND_RESPONSE;
	}
	if (request->state == SEND_RESPONSE)
	{
		// printf("Initiating Send Response\n");

		while (1)
		{
			int len = write(request->mysocketclient, &request->buf_read[request->totalByetsWrittenToClient], request->totalBytesToWriteToServer);

			if (len < 0)
			{
				if (errno == EWOULDBLOCK ||
					errno == EAGAIN)
				{
					// no more data to be read
				}
				else
				{
					perror("server write");
					close(request->mysocketclient);
					close(request->mysocketweb);
					free(request);
				}
				return;
			}

			request->totalByetsWrittenToClient += len;

			if (request->totalByetsWrittenToClient >= request->totalBytesToWriteToServer)
			{
				break;
			}
		}

		close(request->mysocketclient);
		close(request->mysocketweb);

		request->state = FINISHED;
	}
	if (request->state == FINISHED)
	{
		fprintf(stderr, "\nFinished\n");
	}
}

void test_parser()
{
	int i;
	char method[16], hostname[64], port[8], path[64], headers[1024];

	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL};

	for (i = 0; reqs[i] != NULL; i++)
	{
		printf("Testing %s\n", reqs[i]);
		if (parse_request(reqs[i], method, hostname, port, path, headers))
		{
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("HEADERS: %s\n", headers);
		}
		else
		{
			printf("REQUEST INCOMPLETE\n");
		}
	}
}

void print_bytes(unsigned char *bytes, int byteslen)
{
	int i, j, byteslen_adjusted;

	if (byteslen % 8)
	{
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	}
	else
	{
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++)
	{
		if (!(i % 8))
		{
			if (i > 0)
			{
				for (j = i - 8; j < i; j++)
				{
					if (j >= byteslen_adjusted)
					{
						printf("  ");
					}
					else if (j >= byteslen)
					{
						printf("  ");
					}
					else if (bytes[j] >= '!' && bytes[j] <= '~')
					{
						printf(" %c", bytes[j]);
					}
					else
					{
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted)
			{
				printf("\n%02X: ", i);
			}
		}
		else if (!(i % 4))
		{
			printf(" ");
		}
		if (i >= byteslen_adjusted)
		{
			continue;
		}
		else if (i >= byteslen)
		{
			printf("   ");
		}
		else
		{
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}
