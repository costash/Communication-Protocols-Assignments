#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "dns_message.h"

using namespace std;

#define DNS "8.8.8.8"
#define DNSPORT 53
#define BUFLEN 1024

#define WAIT 2		// 2 seconds waiting time

/* Error message + interpretation of the code */
void error(string msg)
{
	perror(msg.c_str());
	exit(1);
}

/* Convert from www.google.com format to
 * 3www6google3com format.
 **/
void convert_to_dns(unsigned char *dns, unsigned char *name)
{
	unsigned int point = 0, i;
	strcat((char *)name, ".");		// Pun punct la sfârșit

	for (i = 0; i < strlen((const char *)name); ++i)
	{
		if (name[i] == '.')
		{
			*dns++ = i - point;		// pentru www. scriu 3
			for (; point < i; ++point)
			{
				*dns++ = name[point];
			}
			++point;
		}
	}
	*dns++ = '\0';
}

/* Make query to a dns server */
bool get_host_by_name(unsigned char *host, unsigned char *server, int query_type)
{
	struct sockaddr_in serv_addr;
	int sockfd;						// Socket for UDP connection
	unsigned char buffer[BUFLEN], *queryname, *read_ptr;
	int readsocks;					// Sockets read by select

	dns_header_t *dns_header = NULL;	// DNS Header
	dns_question_t *dns_question = NULL;	// DNS Question

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(DNSPORT);
	inet_aton((char *)server, &serv_addr.sin_addr);	// DNS address

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	dns_header = (dns_header_t *) buffer;
	memset(dns_header, 0, sizeof(dns_header));

	dns_header->id = (unsigned short) htons(getpid());
	dns_header->qdcount = htons(1);
	dns_header->rd = 1;

	cerr << endl << ntohs(dns_header->id) << " " << ntohs(dns_header->qdcount) << " ";
	cerr << (unsigned short int)dns_header->rd << endl;

	queryname = (unsigned char*) &buffer[sizeof(dns_header_t)];

	cerr << host << endl;
	convert_to_dns(queryname, host);
	cerr << "\n" << queryname << endl << host << endl;

	dns_question = (dns_question_t *) &buffer[ sizeof(dns_header_t)
	                                           + strlen((const char*)queryname) + 1 ];

	dns_question->qclass = htons(1);	// IN class for Internet
	dns_question->qtype = htons(query_type);

	cerr << "qclas: " << ntohs(dns_question->qclass) << " qtype: "
			<< ntohs(dns_question->qtype) << endl;

	cerr << "Sending query for " << host << "...\n";
	if (sendto(sockfd, (char *)buffer, sizeof(dns_header_t)
			+ (strlen((const char*)queryname) + 1) + sizeof(dns_question_t),
			0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Error in sendto");
		return true;
	}
	cerr << "Done sending.\n";

	fd_set readfds;
	struct timeval tv;

	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);

	tv.tv_sec = WAIT;
	tv.tv_usec = 0;

	readsocks = select(sockfd + 1, &readfds, NULL, NULL, &tv);

	if ( readsocks == -1 )
	{
		perror("Error in select");
		return true;
	}
	if ( readsocks == 0 )
	{
		cerr << "Timeout in select\n";
		return true;
	}
	if ( FD_ISSET(sockfd, &readfds) )
	{
		int i = sizeof(serv_addr);
		memset(buffer, 0, sizeof(buffer));
		cerr << "Receiving RR...\n";
		if ( recvfrom(sockfd, (char *)buffer, BUFLEN, 0, (struct sockaddr*) &serv_addr,
				(socklen_t*) &i) < 0 )
		{
			perror("Error in recvfrom");
			return true;
		}
		cerr << "Ans received.\n";

		dns_header = (dns_header_t*) buffer;

		read_ptr = &buffer[ sizeof(dns_header_t) + (strlen((const char*)queryname))
		                    + 1 + sizeof(dns_question_t) ];

		cerr << "\nResponse has:\n";
		cerr << ntohs(dns_header->qdcount) << " Questions.\n";
		cerr << ntohs(dns_header->ancount) << " Answers.\n";
		cerr << ntohs(dns_header->nscount) << " Authoritative servers\n";
		cerr << ntohs(dns_header->arcount) << " Additional records\n";
	}

	return false;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		cerr << "Usage: ./my_dns_client hostname/address type\n";
		exit(0);
	}

	cerr << argv[1] << " " << argv[2] << endl << endl;


	int sockfd;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(DNSPORT);
	inet_aton(DNS, &serv_addr.sin_addr);


	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	cerr << "Am deschis socket-ul " << sockfd << endl;

	memset(buffer, 0, sizeof(buffer));

	dns_header_t header;
	dns_question_t question;
	memset(&header, 0, sizeof(header));
	memset(&question, 0, sizeof(question));

	header.qdcount = htons(1);
	header.rd = 1;

	question.qtype = htons(A);
	question.qclass = htons(1);	// IN

	memcpy(buffer, &header, sizeof(header));

	unsigned char nume[BUFLEN];
	unsigned char goo[BUFLEN] = "www.google.com\0";
	memset(nume, 0, sizeof(nume));
	convert_to_dns(nume, goo);

	cerr << nume << endl;
	cerr << goo << endl;

	memcpy(buffer + sizeof(header), nume, (strlen((const char*)nume) + 1));

	memcpy(buffer + sizeof(header) + (strlen((const char*)nume) + 1), &question, sizeof(question));

	sendto(sockfd, buffer, sizeof(header) + (strlen((const char*)nume) + 1) + sizeof(question), 0,
			(struct sockaddr*) &serv_addr, sizeof(serv_addr));


	cerr << "Query sent for: " << goo;

	get_host_by_name((unsigned char*)argv[1], (unsigned char*)DNS, A);

	return 0;
}
