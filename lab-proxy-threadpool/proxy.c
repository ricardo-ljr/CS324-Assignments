#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NTHREADS 8
#define SBUFSIZE 5

// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);
int open_sfd(char *port);
void *handle_client(void *vargp);

sbuf_t sbuf;

int main(int argc, char *argv[])
{
	// test_parser();
	// printf("%s\n", user_agent_hdr);

	struct sockaddr_storage clientaddr; // peer_addr
	socklen_t clientlen;

	int connfd;
	pthread_t tid;

	int sfd = open_sfd(argv[1]);

	sbuf_init(&sbuf, SBUFSIZE);

	for (int i = 0; i < NTHREADS; i++) // number of threads is 8
	{
		pthread_create(&tid, NULL, handle_client, NULL);
	}

	while (1)
	{
		clientlen = sizeof(struct sockaddr_storage);
		connfd = accept(sfd, (struct sockaddr *)&clientaddr, &clientlen);
		sbuf_insert(&sbuf, connfd);
	}

	return 0;
}

// It returns 1 if all headers have been received (i.e., end-of-headers sequence is found) and 0 otherwise.
int all_headers_received(char *request) // null-terminated string containing an HTTP request
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

int open_sfd(char *port)
{

	struct addrinfo hints;
	struct addrinfo *result;
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

	if ((sfd = socket(result->ai_family, result->ai_socktype, 0)) < 0)
	{
		perror("Error creating socket");
		exit(EXIT_FAILURE);
	}

	if (bind(sfd, result->ai_addr, result->ai_addrlen) < 0)
	{
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result); /* No longer needed */

	listen(sfd, 100);

	return sfd;
}

void *handle_client(void *vargp)
{

	pthread_detach(pthread_self());

	while (1)
	{
		int new_socket = sbuf_remove(&sbuf);

		free(vargp);

		ssize_t nread;
		char buf[MAX_OBJECT_SIZE];
		char buf1[MAX_OBJECT_SIZE];

		bzero(buf, MAX_OBJECT_SIZE);
		bzero(buf1, MAX_OBJECT_SIZE);

		// int index;

		// while (all_headers_received(buf) == 0) // \r\n\r\n
		// {
		// 	nread = read(new_socket, &buf[index], MAX_OBJECT_SIZE);
		// 	index += nread;
		// }

		while (strstr(buf, "\r\n\r\n") == NULL)
		{
			nread = recv(new_socket, buf1, MAX_OBJECT_SIZE, 0);

			if (nread == -1)
			{
				printf("I failed\n");
			}
			else if (nread == 0)
			{
				close(new_socket);
			}
			else
			{
				strcat(buf, buf1);
			}
		}

		char method[16], hostname[64], port[8], path[64], headers[1024];

		// printf("Here my buf %s\n", buf);

		if (parse_request(buf, method, hostname, port, path, headers))
		{

			// 	GET /cgi-bin/slowsend.cgi?obj=lyrics HTTP/1.0
			// Host: www-notls.imaal.byu.edu:5599
			// User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0
			// Connection: close
			// Proxy-Connection: close
			char new_request[MAX_OBJECT_SIZE];

			bzero(new_request, MAX_OBJECT_SIZE);
			strcat(new_request, method);
			// strcat(new_request, " ");
			strcat(new_request, path);
			// strcat(new_request, " ");
			strcat(new_request, "HTTP/1.0\r\n");

			// strcat(new_request, "Host: ");
			// strcat(new_request, hostname);

			// if (strcmp(port, "80") != 0)
			// {
			// 	strcat(new_request, ":");
			// 	strcat(new_request, port);
			// }

			// strcat(new_request, "\r\n");

			// strcat(new_request, user_agent_hdr);
			// strcat(new_request, "\r\n");

			strcat(new_request, headers);

			printf("Header me: %s\n", headers);
			strcat(new_request, "Connection: close\r\n");
			strcat(new_request, "Proxy-Connection: close\r\n\r\n");

			// printf("I'm working boi\n");
			printf("My new request: %s\n\n", new_request);

			int sfd, s;
			struct addrinfo hints, *results, *rp;

			char host[64];

			if (strcmp(port, "80") != 0)
			{
				char *in = strchr(hostname, ':');
				int w = in - hostname;
				strncpy(host, hostname, w);
			}
			else
			{
				strcpy(host, hostname);
			}

			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = 0;
			hints.ai_protocol = 0; /* Any protocol */

			if ((s = getaddrinfo(host, port, &hints, &results)) != 0)
			{
				fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", host, port, gai_strerror(s));
				// exit(EXIT_FAILURE);
				// return;
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

				// if (close(sfd) < 0) {
				//  print error?
				// }
			}

			// Send request to server

			freeaddrinfo(results);
			int send_len = strlen(new_request);
			send(sfd, new_request, send_len, 0);

			bzero(new_request, MAX_OBJECT_SIZE);
			int index = 0; // num bytes

			while (nread > 0)
			{
				nread = read(sfd, &new_request[index], MAX_OBJECT_SIZE);
				index += nread;
			}

			close(sfd);
			send(new_socket, new_request, index, 0);
			close(new_socket);
		}
		else
		{
			printf("REQUEST INCOMPLETE\n");
		}
	}

	return NULL;
}

int parse_request(char *request, char *method,
				  char *hostname, char *port, char *path, char *headers)
{
	char methodParse = ' ';
	char *hostnameParse = "Host: ";
	char portParse = ':';
	char pathParse = '/';
	// char headerParse = "\r\n";

	// printf("I'm starting boi\n\n");

	if (!all_headers_received(request))
	{
		// printf("Incomplete\n");
		return 0;
	}

	// Method
	char *reqs = strchr(request, methodParse);
	// printf("Reqs here: %s\n", reqs);
	int methodPosition = (reqs - request); // Get position of first space
	// printf("Position here: %d\n", methodPosition);
	bzero(method, 16);							 // erase memory
	memcpy(method, request, methodPosition + 1); // copy values from method to request
	// printf("Method here: %s\n", method);

	// HostName
	char *host = strstr(request, hostnameParse);
	// printf("Host here: %s\n", host);

	int i = 0;
	while (host[i] != '\r')
	{
		i++;
	}

	bzero(hostname, 64);
	strncpy(hostname, &host[6], i - 6);
	// printf("Host name: %s\n", hostname);

	// Port - if no string specified, should be 80 by default

	char *portToParse = strchr(hostname, portParse);
	// printf("Port me: %s\n", portToParse);

	if (portToParse != NULL)
	{
		bzero(port, 8);
		strncpy(port, &portToParse[1], strlen(portToParse));
		// printf("Port is here: %s\n", port);
	}
	else
	{
		bzero(port, 8);
		memcpy(port, "80", 2);
		// printf("Port is here: %s\n", port);
	}

	// Path

	char *pathToParse = strstr(request, hostname);
	// printf("Path me: %s\n", pathToParse);

	char *pathToParseAgain = strchr(pathToParse, pathParse); // '/'

	char *pathToParseAgainSpace = strchr(pathToParseAgain, ' ');
	int pos = (pathToParseAgainSpace - pathToParseAgain);

	bzero(path, 64); // erase memory
	memcpy(path, pathToParseAgain, pos + 1);
	// printf("Path here boi: %s\n", path);

	// Headers

	char *parseToHeader = strstr(request, "\r\n");
	strncpy(headers, &parseToHeader[2], strlen(parseToHeader));

	// printf("Header me: %s\n", headers);

	return 1; // If successful I'm assuming
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
