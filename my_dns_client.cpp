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
#define BUFLEN 256

void error(string msg)
{
	perror(msg.c_str());
	exit(1);
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
	strcpy(buffer, "Tralala testing");
	sendto(sockfd, buffer, strlen(buffer) + 1, 0,
			(struct sockaddr*) &serv_addr, sizeof(serv_addr));

	return 0;
}
