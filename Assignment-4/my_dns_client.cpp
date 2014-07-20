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

#define DNS "127.0.0.1"
#define DNSPORT 53
#define BUFLEN 1024
#define NAMESZ 256

#define WAIT 2		// 2 seconds waiting time

ofstream fout("logfile");

/* Error message + interpretation of the code */
void error(string msg)
{
	perror(msg.c_str());
	exit(1);
}

/* Convert type from int to string
 * e.g.: 1 becomes A; 2 becomes NS
 */
string type_to_string(int type)
{
	switch(type)
	{
	case A: return "A";
	case NS: return "NS";
	case CNAME: return "CNAME";
	case PTR: return "PTR";
	case MX: return "MX";
	default: return "";
	}
}

/*
 * Convert type 1 to string "IN"
 */
string class_type_to_string(int type)
{
	if (type == 1)
		return "IN";
	return "";
}

/* Convert type from char* to int
 * e.g.: "A" becomes 1, "NS" becomes 2
 */
int string_to_type(char *type)
{
	if ( strcmp(type, "A") == 0 )
		return A;
	if ( strcmp(type, "NS") == 0 )
		return NS;
	if ( strcmp(type, "CNAME") == 0)
		return CNAME;
	if ( strcmp(type, "PTR") == 0)
		return PTR;
	if ( strcmp(type, "MX") == 0)
		return MX;
	return -1;
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

/* Convert from 3www6google3.com format to
 * www.google.com format
 */
unsigned char* dns_to_host(unsigned char* read_ptr, unsigned char* buffer, int& count)
{
	unsigned char *name;

	if ( (name = (unsigned char*) calloc(NAMESZ, sizeof(unsigned char))) < 0 )
		error("malloc error in dns_to_host");

	unsigned int poz = 0, offset;
	bool jumped = false;
	int i, j;

	count = 1;
	name[0] = '\0';

	// Citesc numele în formatul 3www6google3com
	while (*read_ptr != 0)
	{
		// Trebuie să sar către un nume
		if (*read_ptr >= 192)
		{
			// 11000000 0000000 e scăzut din numărul format de
			// cele 2 caractere
			offset = (*read_ptr) * (1<<8) + *(read_ptr + 1)
					- (((1<<16) - 1) - ((1<<14) -1));

			read_ptr = buffer + offset - 1;
			jumped = true;	// Nu voi mai crește count-ul
		}
		else
		{
			name[poz++] = *read_ptr;
		}
		read_ptr += 1;	// Trec la următorul caracter

		if ( !jumped )
			count++;
	}

	name[poz] = '\0';	// Sfârșit de șir
	if (jumped)
	{
		count++;
	}

	// Conversia propriu-zisă
	for (i = 0; i < (int)strlen((const char*)name); ++i)
	{
		poz = name[i];
		for (j = 0; j < (int)poz; ++j)
		{
			name[i] = name[i + 1];
			++i;
		}
		name[i] = '.';
	}
	name[i] = '\0';

	return name;
}

/* Make query to a dns server */
bool get_host_by_name(unsigned char *host2, unsigned char *server, int query_type)
{
	struct sockaddr_in serv_addr, ans;
	int sockfd;						// Socket for UDP connection
	unsigned char buffer[BUFLEN], *queryname, *read_ptr;
	int readsocks;					// Sockets read by select

	unsigned char host[NAMESZ];
	strcpy((char *)host, (const char*)host2);

	dns_header_t *dns_header = NULL;	// DNS Header
	dns_question_t *dns_question = NULL;	// DNS Question

	memset(buffer, 0, sizeof(buffer));
	/* ==========================================================================
	 * Socket + dns header handling
	 */
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

	queryname = (unsigned char*) &buffer[sizeof(dns_header_t)];

	/* ============================================================================
	 * Începe trimiterea unui query dns
	 */
	fout << "\n; Trying: " << host << " " << type_to_string(query_type);

	if (query_type != PTR)
		convert_to_dns(queryname, host);
	else
	{
		struct in_addr temp;
		inet_aton((const char*)host, &temp);

		unsigned int ptr = ntohl(*(unsigned int*)&temp);
		temp = *(struct in_addr*)&ptr;


		strcat((char*)host, ".in-addr.arpa");
		sprintf((char*)host,"%s.in-addr.arpa", inet_ntoa(temp) );

		convert_to_dns(queryname, host);
	}

	dns_question = (dns_question_t *) &buffer[ sizeof(dns_header_t)
	                                           + strlen((const char*)queryname) + 1 ];

	dns_question->qclass = htons(1);	// IN class for Internet
	dns_question->qtype = htons(query_type);

	if (sendto(sockfd, (char *)buffer, sizeof(dns_header_t)
			+ (strlen((const char*)queryname) + 1) + sizeof(dns_question_t),
			0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Error in sendto");
		return true;
	}

	/* Aștept un timp pentru a primi răspuns de la server și apoi
	 * scriu răspunsul în fișier. Dacă nu am primit răspuns trec
	 * mai departe
	 */

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
		/* Am primit răspuns de la server și încep să-l parsez.
		 * Scriu în fișier răspunsul
		 */
		int i = sizeof(serv_addr), j;
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

		// Sar peste header și query
		read_ptr = &buffer[ sizeof(dns_header_t) + (strlen((const char*)queryname))
		                    + 1 + sizeof(dns_question_t) ];

		// -------------------------------------------------------------------
		// Secțiunea Answer
		int last = 0;
		RR answer;
		if (ntohs(dns_header->ancount) > 0)
			fout << "\n\n;; ANSWER SECTION:";

		for (i = 0; i < ntohs(dns_header->ancount); ++i)
		{
			memset(&answer, 0, sizeof(answer));
			answer.name = dns_to_host(read_ptr, buffer, last);
			read_ptr += last;

			answer.resource = (dns_rr_t *)read_ptr;
			read_ptr += sizeof(dns_rr_t);

			// -----------------------
			// Afișez numele în fișier
			if (type_to_string(ntohs(answer.resource->type)) == "")
				continue;
			fout << "\n" << answer.name << "\t"
				 << class_type_to_string(ntohs(answer.resource->class_))
				 << "\t" << type_to_string(ntohs(answer.resource->type));

			if (ntohs(answer.resource->type) == A)	// IP address
			{
				unsigned short len = ntohs(answer.resource->rdlength);
				if ( (answer.rdata = (unsigned char*) malloc(len)) < 0)
					error("Malloc error in answer section");

				for (j = 0; j < ntohs(answer.resource->rdlength); ++j)
				{
					answer.rdata[j] = read_ptr[j];
				}

				answer.rdata[len] = '\0';
				read_ptr += len;

				memset(&ans, 0, sizeof(ans));
				long *point;
				point = (long*)answer.rdata;
				ans.sin_addr.s_addr = (*point);

				fout << "\t" << inet_ntoa(ans.sin_addr);
			}

			if (ntohs(answer.resource->type) == MX)	// Mail
			{
				unsigned short *pref = NULL;
				pref = (unsigned short *) read_ptr;
				read_ptr += 2;

				unsigned short len = ntohs(answer.resource->rdlength);
				if ( (answer.rdata = (unsigned char*) malloc(len)) < 0)
					error("Malloc error in answer section");

				answer.rdata = dns_to_host(read_ptr, buffer, last);
								read_ptr += last;

				fout << "\t" << ntohs(*pref) << "\t" << answer.rdata;
			}

			else
			{
				answer.rdata = dns_to_host(read_ptr, buffer, last);
				read_ptr += last;

				fout << "\t" << answer.rdata;
			}
		}

		// Secțiunea Authorities
		if (ntohs(dns_header->nscount) > 0)
			fout << "\n\n;; AUTHORITY SECTION:";
		for (i = 0; i < ntohs(dns_header->nscount); ++i)
		{
			memset(&answer, 0, sizeof(answer));
			answer.name = dns_to_host(read_ptr, buffer, last);
			read_ptr += last;

			answer.resource = (dns_rr_t*)read_ptr;
			read_ptr += sizeof(dns_rr_t);

			answer.rdata = dns_to_host(read_ptr, buffer, last);
			read_ptr += last;

			if (type_to_string(ntohs(answer.resource->type)) == "")
				continue;
			fout << "\n" << answer.name << "\t"
				 << class_type_to_string(ntohs(answer.resource->class_))
				 << "\t" << type_to_string(ntohs(answer.resource->type))
				 << "\t" << answer.rdata;
		}

		// Secțiunea Additional
		if (ntohs(dns_header->arcount) > 0)
			fout << "\n\n;; ADDITIONAL SECTION:";
		for (i = 0; i < ntohs(dns_header->arcount); ++i)
		{
			memset(&answer, 0, sizeof(answer));
			answer.name = dns_to_host(read_ptr, buffer, last);
			read_ptr += last;

			answer.resource = (dns_rr_t*)read_ptr;
			read_ptr += sizeof(dns_rr_t);

			if (type_to_string(ntohs(answer.resource->type)) == "")
				continue;
			fout << "\n" << answer.name << "\t"
				 << class_type_to_string(ntohs(answer.resource->class_))
				 << "\t" << type_to_string(ntohs(answer.resource->type));

			if (ntohs(answer.resource->type) == A)	// IP address
			{
				unsigned short len = ntohs(answer.resource->rdlength);
				if ( (answer.rdata = (unsigned char*) malloc(len)) < 0)
					error("Malloc error in answer section");

				for (j = 0; j < ntohs(answer.resource->rdlength); ++j)
				{
					answer.rdata[j] = read_ptr[j];
				}

				answer.rdata[len] = '\0';
				read_ptr += len;

				memset(&ans, 0, sizeof(ans));
				long *point;
				point = (long*)answer.rdata;
				ans.sin_addr.s_addr = (*point);

				fout << "\t" << inet_ntoa(ans.sin_addr);
			}

			else
			{
				answer.rdata = dns_to_host(read_ptr, buffer, last);
				read_ptr += last;

				fout << "\t" << answer.rdata;
			}
		}
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

	ifstream fin("dns_servers.conf");
	char buf[NAMESZ];
	bool ok = false;

	while (fin.good() && ok)
	{
		fin.getline(buf, NAMESZ);

		if (buf[0] == '#' || buf[0] == '\0')
			continue;

		ok = get_host_by_name((unsigned char*)argv[1], (unsigned char*)buf, string_to_type(argv[2]));
	}

	get_host_by_name((unsigned char*)argv[1], (unsigned char*)DNS, string_to_type(argv[2]));

	fin.close();
	fout.close();

	return 0;
}
