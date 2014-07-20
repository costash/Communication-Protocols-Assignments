#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFLEN 256

void error(char *msg)
{
	perror(msg);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	fd_set read_fds;	//multimea de citire folosita in select()
	fd_set tmp_fds;	//multime folosita temporar
	int fdmax;		//valoare maxima file descriptor din multimea read_fds

	char buffer[BUFLEN];
	if (argc < 4) {
		fprintf(stderr,"Usage %s client_name server_address server_port\n", argv[0]);
		exit(0);
	}

	//golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket at client");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	inet_aton(argv[2], &serv_addr.sin_addr);


	if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0)
		error("ERROR connecting to server");

	while(1){
		//citesc de la tastatura
		memset(buffer, 0 , BUFLEN);
		fgets(buffer, BUFLEN-1, stdin);

		//trimit mesaj la server
		n = send(sockfd,buffer,strlen(buffer), 0);
		if (n < 0)
			 error("ERROR writing to socket");

	}
	return 0;
}


