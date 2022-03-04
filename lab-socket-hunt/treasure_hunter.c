// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 0x6CB368C2
// hex value of my UserID: 0x6cb368c2

#define BUFSIZE 8
// Nonce: 07 4F  7C F7

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

// int verbose = 0;

/* PROBLEMS: 
1. When I increment 0XFF, rather than giving 0x100, it returns 0x0, which ends up sending the wrong nonce value - Fixed
2. My port is being unrecogninzed on level three even though it=s sending the correct one - Fixed
3. Why is level 0 not printing the correct output on certain seeds? - Fixed
*/

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[])

{

	// Inializing values to dynamically update
	unsigned int zero = htons(0x0); // TODO: Remove buff and build dynamic buffer
	unsigned int level = atoi(argv[3]);
	unsigned short s1 = atoi(argv[4]); // Seed
	unsigned short s2 = htonl(s1);	   // Changing seed
	// unsigned short s2 = htons(s1);	   // This doesn't work on my user id

	unsigned int user_id = htonl(0x6cb368c2);

	// unsigned char buf[64];
	unsigned char *buf = malloc(1024);

	// converstin see from string ton int to hex network byte order

	// unsigned char buf[64] = {0, atoi(argv[3]), 0x6c, 0xb3, 0x68, 0xc2}; // Initial buffer

	memcpy(&buf[0], &zero, 1);
	memcpy(&buf[1], &level, 1);
	memcpy(&buf[2], &user_id, 4);
	memcpy(&buf[6], &s2, 2);

	// Dynamic update to initial buf works
	// for (int k = 0; k < 8; k++)
	// {
	// 	printf("%x ", buf[k]);
	// }

	// printf("\n");

	// unsigned char buf2[64];
	// unsigned char treasure[1024];

	unsigned char *buf2 = malloc(1024); // initializing new buffer to receive message, does this work better?
	// unsigned char *buf2[64] = {0, 0, 0, 0};

	// int treasureIndex = 0;

	// struct addrinfo hints;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	// unsigned char *port[64]; // not working?
	char *port = malloc(128);

	int s, sfd;

	unsigned int currPort1;

	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;		/* Allow IPv4, IPv6, or both, depending on what was specified on the command line. */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	s = getaddrinfo(argv[1], argv[2], &hints, &result); // address, port, hints, result

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
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
		{
			struct sockaddr_in local;
			unsigned int addr_len = sizeof(local);
			getsockname(sfd, (struct sockaddr *)&local, &addr_len);
			currPort1 = local.sin_port; // Get curr port working?
			break;						/* Success */
		}

		close(sfd);
	}

	if (rp == NULL)
	{ /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}

	send(sfd, buf, 8, 0); // sending 8 bytes
	recv(sfd, buf2, 64, 0);

	// free(buf);
	freeaddrinfo(result);

	// unsigned int nonce;
	unsigned char *treasure = malloc(1024); // keep track of treasure
	int treasureLength = 0;

	int chunkSize = (int)buf2[0];

	// printf("Chunk size: %d\n", chunkSize);

	int opCode;
	char *whichIP = "IPv4";

	while (chunkSize != 0)
	{

		if (buf2[0] > 127)
		{
			if (buf2[0] == 129)
			{
				printf("The message was sent from an unexpected address or port\n");
				break;
			}
			else if (buf2[0] == 130)
			{
				printf("Message had an incorrect length\n");
				break;
			}
			else if (buf2[0] == 131)
			{
				printf("The value of the nonce was incorrect\n");
				break;
			}
			else if (buf2[0] == 133)
			{
				printf("Multiple tries and the server was unable to bind properly to the address and port\n");
				break;
			}
		}

		// printf("Chunk size: %d\n", chunkSize);

		memcpy(&treasure[treasureLength], &buf2[1], chunkSize);
		treasureLength += chunkSize;

		opCode = (int)buf2[chunkSize + 1];

		buf2[chunkSize + 7]++; // TODO: Fix why when it increases from FF to 0 it throws a major error in the code

		// if (buf2[chunkSize + 7] == 0)
		// {
		// 	buf2[chunkSize + 7] = htons(0x100);
		// }

		if (opCode == 1)
		{
			struct addrinfo *new_results;
			unsigned short newPort;
			char *newPortvals = malloc(128);

			memcpy(&newPort, &buf2[chunkSize + 2], 2);
			snprintf(newPortvals, 128, "%u", ntohs(newPort));

			s = getaddrinfo(argv[1], newPortvals, &hints, &new_results);

			for (rp = new_results; rp != NULL; rp = rp->ai_next)
			{
				if (sfd == -1)
				{
					continue;
				}

				if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
				{
					port = newPortvals;
					break;
				}

				close(sfd);
			}
		}
		if (opCode == 2)
		{

			close(sfd); // TODO: Check if I need this. yes

			struct addrinfo *new_results;
			struct sockaddr_in ipv4addr;
			struct sockaddr_in6 ipv6addr;
			unsigned short newPort;

			memcpy(&newPort, &buf2[chunkSize + 2], 2); // Reassigning port

			if (strcmp(whichIP, "IPv4") == 0)
			{
				ipv4addr.sin_family = AF_INET;
				ipv4addr.sin_port = newPort;
				hints.ai_family = AF_INET;
			}
			else
			{
				ipv6addr.sin6_family = AF_INET6;
				ipv6addr.sin6_addr = in6addr_any;
				ipv6addr.sin6_port = newPort;
				hints.ai_family = AF_INET6;
			}

			sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

			if (strcmp(whichIP, "IPv4") == 0)
			{
				bind(sfd, (struct sockaddr *)&ipv4addr, sizeof(ipv4addr));
			}
			else
			{
				bind(sfd, (struct sockaddr *)&ipv6addr, sizeof(ipv6addr));
			}

			s = getaddrinfo(argv[1], port, &hints, &new_results);

			for (rp = new_results; rp != NULL; rp = rp->ai_next)
			{

				if (sfd == -1)
				{
					continue;
				}

				if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
				{
					currPort1 = newPort;
					break;
				}

				close(sfd);
			}
		}

		if (opCode == 3)
		{
			//but also to extract the number of datagrams to immediately receive from the server,
			// as well as receive and sum the payloads of those datagrams, in order to get the nonce.

			close(sfd);

			unsigned int newNonce = 0;

			struct sockaddr_in ipv4addr;
			struct sockaddr_in6 ipv6addr;
			struct sockaddr_in ipv4addr2;
			struct sockaddr_in6 ipv6addr2;

			unsigned short numDatagrams;

			memcpy(&numDatagrams, &buf2[chunkSize + 2], 2);

			sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

			if (strcmp(whichIP, "IPv4") == 0)
			{
				ipv4addr.sin_family = AF_INET;
				ipv4addr.sin_port = currPort1;
				hints.ai_family = AF_INET;
				bind(sfd, (struct sockaddr *)&ipv4addr, sizeof(ipv4addr));
			}
			else
			{
				ipv6addr.sin6_family = AF_INET6;
				ipv6addr.sin6_addr = in6addr_any;
				ipv6addr.sin6_port = currPort1;
				hints.ai_family = AF_INET6;
				bind(sfd, (struct sockaddr *)&ipv6addr, sizeof(ipv6addr));
			}

			int len = sizeof(struct sockaddr_storage);
			bzero(&buf2[0], 64);

			for (int i = 0; i < ntohs(numDatagrams); i++)
			{
				if (strcmp(whichIP, "IPv4") == 0)
				{
					recvfrom(sfd, &buf2[0], 64, 0, (struct sockaddr *)&ipv4addr2, (socklen_t *)&len);
					// printf("%d\n", htons(ipv4addr2.sin_port));
					newNonce += htons(ipv4addr2.sin_port);
				}
				else
				{
					recvfrom(sfd, &buf2[0], 64, 0, (struct sockaddr *)&ipv6addr2, (socklen_t *)&len);
					// printf("%d\n", htons(ipv6addr2.sin6_port));
					newNonce += htons(ipv6addr2.sin6_port);
				}
			}

			newNonce++;
			// printf("%d\n", newNonce);

			int networkNonce = ntohl(newNonce); // TODO:

			unsigned char *newNonceVals = malloc(128);

			memcpy(&newNonceVals[0], &networkNonce, 4);

			connect(sfd, rp->ai_addr, (socklen_t)len);
			send(sfd, (&newNonceVals[0]), 4, 0);
			bzero(&buf2[0], 64);
			recv(sfd, &buf2[0], 64, 0);

			free(newNonceVals);
			chunkSize = (unsigned int)buf2[0];
			continue;
		}
		if (opCode == 4)
		{

			// Switch address families from using IPv4 (AF_INET) to IPv6 (AF_INET6) or vice-versa, and use the port specified
			// by the next two bytes (n + 2 and n + 3), which is an unsigned short in network byte order, as the new remote port
			// (i.e., like op-code 1).

			close(sfd);
			struct addrinfo *new_results;

			unsigned short newPort;

			char *newPortVal = malloc(128);

			memcpy(&newPort, &buf2[chunkSize + 2], 2);
			snprintf(newPortVal, 128, "%u", ntohs(newPort));

			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_flags = 0;
			hints.ai_protocol = 0;

			// Switching address families
			if (strcmp(whichIP, "IPv6") == 0)
			{
				hints.ai_family = AF_INET;
				whichIP = "IPv4";
			}
			else
			{
				hints.ai_family = AF_INET6;
				whichIP = "IPv6";
			}

			// Here you must call getaddrinfo(), and you must create a new socket with socket()

			s = getaddrinfo(argv[1], newPortVal, &hints, &new_results);

			// if (s != 0)
			// {
			// 	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
			// 	exit(EXIT_FAILURE);
			// }

			// New socket
			for (rp = new_results; rp != NULL; rp = rp->ai_next)
			{
				sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

				if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
				{
					port = newPortVal;

					// Switching bettwen IPv4 and IPv6
					if (strcmp(whichIP, "IPv4") == 0)
					{
						struct sockaddr_in local;
						unsigned int addr_len = sizeof(local);
						getsockname(sfd, (struct sockaddr *)&local, &addr_len);
						currPort1 = local.sin_port;
					}
					else
					{
						struct sockaddr_in6 local;
						unsigned int addr_len = sizeof(local);
						getsockname(sfd, (struct sockaddr *)&local, &addr_len);
						currPort1 = local.sin6_port;
					}
					break;
				}
				close(sfd);
			}
		}

		// for (int k = chunkSize + 4; k < (chunkSize + 8); k++)
		// {
		// 	printf("%x ", buf2[k]);
		// }

		// printf("\n");

		send(sfd, &buf2[chunkSize + 4], 4, 0);

		bzero(&buf2[0], 64);
		recv(sfd, &buf2[0], 64, 0);

		chunkSize = (unsigned int)buf2[0];

		// printf("Chunksize %d\n", chunkSize);

		// for (int k = chunkSize + 4; k < (chunkSize + 8); k++)
		// {
		// 	printf("%x ", buf2[k]);
		// }

		// printf("\n");

		// print_bytes(buf2, 64);
	}

	fprintf(stdout, "%.*s\n", treasureLength, treasure);
	free(treasure);
	free(buf2);

	close(sfd); // The end?
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
