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

void error(string msg)
{
	perror(msg.c_str());
	exit(1);
}

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

int main(int argc, char *argv[])
{
	cerr << "Hello world\n";

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
	memset(nume, 0, sizeof(nume));
	nume[0] = 3;
	for (int i = 0; i < 3; ++i)
		nume[i + 1] = 'w';
	nume[4] = 6;
	string google = "google";
	for (int i = 0; i < 6; ++i)
		nume[i+5] = google[i];
	nume[11] = 3;
	nume[12] = 'c';
	nume[13] = 'o';
	nume[14] = 'm';
	nume[15] = '\0';

	unsigned char goo[BUFLEN] = "cs.curs.pub.ro\0";
	memset(nume, 0, sizeof(nume));
	convert_to_dns(nume, goo);

	cerr << nume << endl;
	cerr << goo << endl;

	memcpy(buffer + sizeof(header), nume, (strlen((const char*)nume) + 1));

	memcpy(buffer + sizeof(header) + (strlen((const char*)nume) + 1), &question, sizeof(question));

	sendto(sockfd, buffer, sizeof(header) + (strlen((const char*)nume) + 1) + sizeof(question), 0,
			(struct sockaddr*) &serv_addr, sizeof(serv_addr));


	cerr << "Query sent for: " << goo;

	return 0;
}
