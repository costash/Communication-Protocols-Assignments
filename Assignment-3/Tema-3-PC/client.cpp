// Constantin Serban-Radoi 323CA
// Tema 3 PC
// Aprilie 2012

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

#define BUFFILE 1024	// Buffer pentru fisiere
#define BUFLEN 256		// Buffer mesaje
#define MAX_CLIENTS 5	// Numar clienti
#define CMDSZ 100
#define NAMESZ 100

using namespace std;
int sockfd, newsockfd;
int listen_sockfd;

int fdmax;			//valoare maxima file descriptor din multimea read_fds

fd_set read_fds;	//multimea de citire folosita in select()
fd_set tmp_fds;		//multime folosita temporar
bool parsed_size = false;

typedef struct client_
{
	string nume;	// Nume client
	string fisier;	// Nume fisier
	int size_fis;	// Size fisier
	int port;		// Port ascultare
	int sock;		// Socket pe care e conectat server-ul
	struct sockaddr_in adresa;	// Adresa clientului
}client;	// Structura pentru client care e server

typedef struct primit_
{
	int sock;			// Socketul pe care am primit
	int size_fis;		// Size fisier
	FILE *fisier;	// Fisier in care scriu
}primit;	// Structura pentru client destinatie

map<int, primit> de_scris;	// Ce am de scris in fisiere
vector<client> clienti;		// Clienti conectati la mine, care sunt server

// Mesaj de eroare
void error(char *msg)
{
	perror(msg);
	exit(0);
}

void quit_message()
{
	fprintf(stderr, "Am primit comanda quit și închid clientul\n");
}

void send_verify(int n)
{
	if (n < 0)
		error((char *)"ERROR writing to socket");
}

// Split a string into 3 tokens
void split_string(string str, string& com, string& param1, string& param2)
{
	string token;
	istringstream iss(str);
	int i = 0;
	while ( getline(iss, token, ' ') )
	{
		switch(i)
		{
		case 0:
			com = token;
			i++;
			break;
		case 1:
			param1 = token;
			i++;
			break;
		case 2:
			param2 = token;
			i++;
			break;
		default:
			break;
		}
	}
}

// Functie care primeste fisier
void parse_recv_file(char *buffer, int sock)
{
	for (unsigned int i = 0; i < clienti.size(); ++i)
		if ( clienti[i].sock == sock && de_scris[sock].size_fis > 0 )
		{

			char bufbig[BUFFILE];
			int n;
			memset(bufbig, 0, BUFFILE);
			if ((n = recv(sock, bufbig, sizeof(bufbig), 0)) <= 0)
			{
				if (n == 0)
				{
					//conexiunea s-a inchis
					printf("client: when parsing recieved message, socket hung up\n");
				} else
				{
					error((char *)"ERROR in recv");
				}
				close(sock);
				cerr << "Problem in connection with client that is server. \n";
			}
			else
			{
				size_t nr_wrote = fwrite(bufbig, sizeof(char), n, de_scris[sock].fisier);
				de_scris[sock].size_fis -= nr_wrote;
				cerr << "Remaining to write: " << de_scris[sock].size_fis << endl;
			}
		}

	return;
}

// Functie parsare mesaje primite de la clientul_care_e_server
void parse_cli_srv(char *mesaj, int sock)
{
	for (unsigned int i = 0; i < clienti.size(); ++i)
		if ( clienti[i].sock == sock )
		{
			char buffer[BUFLEN];
			int n;
			memset(buffer, 0, BUFLEN);
			if ((n = recv(sock, buffer, sizeof(buffer), 0)) <= 0)
			{
				if (n == 0)
				{
					//conexiunea s-a inchis
					printf("client: when parsing recieved message, socket hung up\n");
				} else
				{
					error((char *)"ERROR in recv");
				}
				close(sock);
				cerr << "Problem in connection with client that is server. \n";
			}
			else
			{
				cerr << "Reply-from-CLI-srv-BUFFER " << buffer << endl;
				char comanda[CMDSZ];
				sscanf(buffer, "%s %*s", comanda);

				cerr << "Comanda este: {" << comanda << "}\n";

				if ( strcmp(comanda, "SIZE") == 0)
				{
					cerr << "SOCKET-PE ---care--fac---recv la client-care-trimite-fisier " << sock << endl;
					int size;
					char nume_fis[NAMESZ];
					sscanf(buffer, "%*s %d %s", &size, nume_fis);

					clienti[i].size_fis = size;

					primit pr;
					char fname[NAMESZ];
					sprintf(fname, "%s_primit", nume_fis);
					pr.fisier = fopen(fname, "wb");
					if ( pr.fisier == NULL )
					{
						cerr << "Fisierul " << fname << " nu a putut fi creat\n";
						return;
					}
					pr.sock = sock;
					pr.size_fis = size;
	//				de_scris.insert(de_scris.begin(), pair<int, primit>(cli_sock, pr));
					de_scris[sock] = pr;
					cerr << "Socketul folosit " << de_scris[sock].sock << "\n";
					cerr << "Size-ul fisierului: " << de_scris[sock].size_fis << "\n";

					//parse_recv_file(buffer, sock);

					return;
				}

			}
		}
	return;
}


// Funție parsare mesaje speciale primite de la server
bool parse_quick_reply(char *mesaj, int sock)
{
	char buffer[BUFLEN];
	int n;
	memset(buffer, 0, BUFLEN);
	if ((n = recv(sock, buffer, sizeof(buffer), 0)) <= 0)
	{
		if (n == 0)
		{
			//conexiunea s-a inchis
			printf("client: when parsing recieved message, socket hung up\n");
		} else
		{
			error((char *)"ERROR in recv");
		}
		close(sock);
		close(listen_sockfd);
		error((char *)"Problem in connection with server. Closing client...");
	}
	else
	{
		cerr << "Reply-from-srv-BUFFER " << buffer << endl;
		char comanda[CMDSZ];
		sscanf(buffer, "%s %*s", comanda);
		cerr << "Comanda primita: {" << comanda << "}\n";

		if (strcmp(comanda, "info-message-NOTOK") == 0)
		{
			cerr << "client: Error clientul " << buffer + strlen("info-message-NOTOK ") << endl;
			return false;
		}
		if (strcmp(comanda, "info-message") == 0)
		{
			char nume_client[NAMESZ];
			char nume[NAMESZ];
			char ip[NAMESZ];
			int port;
			sscanf(buffer, "%*s %s %s %s %d", nume, nume_client, ip, &port);
//			cerr << "numele-meu: " << nume << " nume client: " << nume_client << " ip: " << ip << " port: " << port << endl;

			// Socket pentru conectare la clientul 2
			int cli2_sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (cli2_sockfd < 0)
				error((char *)"ERROR opening socket at client connecting to another client");

			struct sockaddr_in client2_addr;
			memset((char *) &client2_addr, 0, sizeof(client2_addr));
			client2_addr.sin_family = AF_INET;
			client2_addr.sin_port = htons(port);
			inet_aton(ip, &client2_addr.sin_addr);

			// Ma conectez la clientul 2
			if (connect(cli2_sockfd, (struct sockaddr*) &client2_addr, sizeof(client2_addr)) < 0)
				error((char *) "ERROR at client connecting to another client");

			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "text-msg %s %s", nume, mesaj);
			cerr << "Mesaj de trimis catre clientul 2: {" << buffer << "}\n";

			// Trimit mesaj in care ii cer fisierul clientului 2
			int n = send(cli2_sockfd, buffer, strlen(buffer), 0);
				send_verify(n);

			close(cli2_sockfd);

			return false;
		}

		if ( strcmp(comanda, "getfile_NOTOK") == 0 )
		{
			char nume_fis[NAMESZ];
			char nume_client[NAMESZ];
			sscanf(buffer, "%*s %s %s", nume_client, nume_fis);
			cerr << "Fisierul " << nume_fis << " de la clientul " << nume_client << " nu a putut fi gasit\n";
			return false;
		}

		if ( strcmp(comanda, "info-getfile") == 0 )
		{
			char nume[NAMESZ];
			char nume_client[NAMESZ];
			char nume_fis[NAMESZ];
			char ip[NAMESZ];
			int port;
			sscanf(buffer, "%*s %s %s %s %s %d", nume, nume_client, nume_fis, ip, &port);
			cerr << "Am primit de la server BUFFER: {" << buffer << "}\n";
			cerr << "Eu: " << nume << " cer " << nume_fis << " de la " << nume_client << " pe portul " << port << endl;

			// Socket pentru conectare la clientul 2
			int cli2_sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if (cli2_sockfd < 0)
				error((char *)"ERROR opening socket at client connecting to another client");

			struct sockaddr_in client2_addr;
			memset((char *) &client2_addr, 0, sizeof(client2_addr));
			client2_addr.sin_family = AF_INET;
			client2_addr.sin_port = htons(port);
			inet_aton(ip, &client2_addr.sin_addr);

			// Ma conectez la clientul 2
			if (connect(cli2_sockfd, (struct sockaddr*) &client2_addr, sizeof(client2_addr)) < 0)
				error((char *) "ERROR at client connecting to another client");

			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "getfile %s %s %s", nume, nume_client, nume_fis);
			cerr << "Mesaj de trimis catre clientul 2: {" << buffer << "}\n";

			client cl;
			cl.nume = nume;
			cl.adresa = client2_addr;
			cl.port = port;
			cl.sock = cli2_sockfd;
			cl.fisier = nume_fis;

			cerr << "SOCKET pe care cer fisierul " << cl.sock << endl;

			bool found;
			for (unsigned int i = 0; i < clienti.size(); ++i)
				if (clienti[i].sock == cli2_sockfd)
				{
					found = true;
					break;
				}
			if (!found)
				clienti.push_back(cl);

			// Trimit mesaj la clientul 2
			int n = send(cli2_sockfd, buffer, strlen(buffer), 0);
			send_verify(n);

			parse_cli_srv(buffer, cli2_sockfd);

			//close(cli2_sockfd);
			return false;
		}
	}
	return false;
}

// Parsare comanda de la tastatura
bool parse_command(char *buffer, char *nume)
{
	string comanda (buffer);
	string com, param1, param2;
	char bufsend[BUFLEN];

	comanda = comanda.substr(0,comanda.find_last_of("\n"));
	split_string(comanda, com, param1, param2);

	// Comanda "listclients"
	if (comanda.compare("listclients") == 0)
	{
		if (param1.compare("") != 0)
		{
			fprintf(stderr, "Wrong command. Usage: listclients\n");
			return false;
		}

		// Trimit un string de forma "listclients nume_client_initiator"
		memset(bufsend, 0, BUFLEN);
		sprintf(bufsend, "listclients %s", nume);

		int n = send(sockfd, bufsend, strlen(bufsend), 0);
		send_verify(n);

		fprintf(stderr, "client: Am trimis comanda listclients catre server\n");
		return true;
	}

	// Comanda "infoclient nume_client"
	if (com.compare("infoclient") == 0)
	{
		if ( (param1.compare("") == 0 && param2.compare("") != 0)
				|| comanda.compare("infoclient ") == 0 || comanda.compare("infoclient") == 0)
		{
			fprintf(stderr, "Wrong command. Usage: infoclient nume_client\n");
			return false;
		}
		fprintf(stderr, "Am primit comanda infoclient pentru ");
		cerr << param1 << endl;

		// Trimit un string de forma "infoclient nume_client_initiator
		// nume_client_despre_care_se_cer_info"
		memset(bufsend, 0, BUFLEN);
		sprintf(bufsend, "infoclient %s %s", nume, param1.c_str());

		int n = send(sockfd, bufsend, strlen(bufsend), 0);
		send_verify(n);

		cerr << "client: Am trimis comanda infoclient " << param1 << " catre server\n";

		return true;
	}

	// Comanda "message nume_client mesaj"
	if (com.compare("message") == 0)
	{
		if ( (param1.compare("") == 0 || param2.compare("") == 0)
				|| comanda.compare("message") == 0 || comanda.compare("message ") == 0)
		{
			fprintf(stderr, "Wrong command. Usage: message nume_client mesaj\n");
			return false;
		}
		char mesaj[BUFLEN];
		string buf = buffer;
		string msgstr = buf.substr(com.size() + param1.size() + 2);
		//cerr << "MSGSTR {" << msgstr << "}\n";

		fprintf(stderr, "Am primit comanda message pentru clientul ");
		cerr << param1 << " \n";

//		cerr << "BUFFER-message {" << buffer << "}\n";
		msgstr.copy(mesaj, msgstr.size() - 1);
		mesaj[msgstr.size() - 1] = '\0';
		cerr << "MESAJ: {" << mesaj << "}\n";

		// Trimit la server un string de forma
		// "message nume_client_initiator nume_client mesaj"
		memset(bufsend, 0, BUFLEN);
		sprintf(bufsend, "message %s %s", nume, param1.c_str());

//		cerr << "BUFSEND-message-to-srv {" << bufsend << "}\n";

		int n = send(sockfd, bufsend, strlen(bufsend), 0);
		send_verify(n);

		cerr << "client: Am trimis comanda " << bufsend << " catre server\n";

		return parse_quick_reply(mesaj, sockfd);
	}

	// Comanda "sharefile nume_fisier"
	if (com.compare("sharefile") == 0)
	{
		if ( (param1.compare("") == 0 && param2.compare("") != 0)
				|| comanda.compare("sharefile") == 0 || comanda.compare("sharefile ") == 0)
		{
			fprintf(stderr, "Wrong command. Usage: sharefile nume_fisier\n");
			return false;
		}
		fprintf(stderr, "Am primit comanda sharefile pentru fisierul ");
		cerr << param1 << "\n";

		ifstream fisier;
		fisier.open(param1.c_str());
		if (!fisier.is_open())
		{
			cerr << "client: Fisierul nu poate fi deschis\n";
			return false;
		}
		else
			fisier.close();

		// Trimit mesaj catre server de forma
		// "sharefile nume_client_initiator nume_fisier"
		memset(bufsend, 0, BUFLEN);
		sprintf(bufsend, "sharefile %s %s", nume, param1.c_str());

		int n = send(sockfd, bufsend, strlen(bufsend), 0);
		send_verify(n);

		cerr << "client: Am trimis comanda " << bufsend << " catre server\n";

		return true;
	}

	// Comanda "unsharefile nume_fisier"
	if (com.compare("unsharefile") == 0)
	{
		if ( (param1.compare("") == 0 && param2.compare("") != 0)
				|| comanda.compare("unsharefile") == 0 || comanda.compare("unsharefile ") == 0)
		{
			fprintf(stderr, "Wrong command. Usage: unsharefile nume_fisier\n");
			return false;
		}
		fprintf(stderr, "Am primit comanda unsharefile pentru fisierul ");
		cerr << param1 << "\n";

		// Trimit mesaj catre server de forma
		// "unsharefile nume_client_initiator nume_fisier"
		memset(bufsend, 0, BUFLEN);
		sprintf(bufsend, "unsharefile %s %s", nume, param1.c_str());

		int n = send(sockfd, bufsend, strlen(bufsend), 0);
		send_verify(n);

		cerr << "client: Am trimis comanda " << bufsend << " catre server\n";
		return true;
	}

	// Comanda "getshare nume_client"
	if (com.compare("getshare") == 0)
	{
		if ( (param1.compare("") == 0 && param2.compare("") != 0)
				|| comanda.compare("getshare") == 0 || comanda.compare("getshare ") == 0)
		{
			fprintf(stderr, "Wrong command. Usage: getshare nume_client\n");
			return false;
		}
		fprintf(stderr, "Am primit comanda getshare pentru clientul ");
		cerr << param1 << "\n";

		// Trimit mesaj catre server de forma
		// "getshare nume_client_initiator nume_client_interogat"
		memset(bufsend, 0, BUFLEN);
		sprintf(bufsend, "getshare %s %s", nume, param1.c_str());

		int n = send(sockfd, bufsend, strlen(bufsend), 0);
		send_verify(n);

		cerr << "client: Am trimis comanda " << bufsend << " catre server\n";
		return true;
	}

	// Comanda "getfile nume_client nume_fisier"
	if (com.compare("getfile") == 0)
	{
		if ( (param1.compare("") == 0 || param2.compare("") == 0)
						|| comanda.compare("getfile") == 0 || comanda.compare("getfile ") == 0)
		{
			fprintf(stderr, "Wrong command. Usage: getfile nume_client nume_fisier\n");
			return false;
		}
		fprintf(stderr, "Am primit comanda getfile pentru clientul ");
		cerr << param1 << " si fisierul " << param2 << "\n";

		// Trimit catre server cerere de informatii pentru clientul interogat de forma
		// "getfile nume_client_interogator nume_client_interogat fisier"
		memset(bufsend, 0, BUFLEN);
		sprintf(bufsend, "getfile %s %s %s", nume, param1.c_str(), param2.c_str());
		cerr << "Buffer-de-trimis-la-srv-getfile: {" << bufsend << "}\n";

		int n = send(sockfd, bufsend, strlen(bufsend), 0);
		send_verify(n);

		char nume_fis[NAMESZ];
		param2.copy(nume_fis, param2.size(), 0);

		// Parsez răspunsul primit de la server
		return parse_quick_reply(nume_fis, sockfd);
	}

	// Comanda "quit"
	if (comanda.compare("quit") == 0)
	{
		quit_message();
		close(sockfd);
		close(listen_sockfd);
		exit(0);
	}

	fprintf(stderr, "Wrong command. Usage: command [param1] [param2]\n");
	return false;

}

// Functie de parsare a mesajelor venite de la server
void parse_message(int sock)
{
	char buffer[BUFLEN];
	char comanda[CMDSZ];
	int n;
	memset(buffer, 0, BUFLEN);
	if ((n = recv(sock, buffer, sizeof(buffer), 0)) <= 0)
	{
		if (n == 0)
		{
			//conexiunea s-a inchis
			printf("client: when parsing recieved message, socket hung up\n");
		} else
		{
			error((char *)"ERROR in recv");
		}
		close(sock);
		error((char *)"Problem in connection with server. Closing client...");
	}
	else
	{
		cerr << "Recv-BUFFER " << buffer << endl;
		stringstream ss (stringstream::in | stringstream::out);
		string buf = buffer;
		ss << buf;
		ss >> comanda;
		cerr << "Comanda: " << comanda << endl;

		// Am primit lista de clienti sub forma: "clienti client1 client2..."
		if (strcmp(comanda, "clienti") == 0)
		{
			cout << "LISTA de clienti este:\n" << buffer + strlen("clienti ") << endl;
			return;
		}

		// Am primit client-inexistent ca reply la clientinfo
		if (strcmp(comanda, "client-inexistent") == 0)
		{
			cout << "Clientul interogat nu exista\n";
			return;
		}

		// Am primit informatiile despre clientul interogat
		if (strcmp(comanda, "info-client") == 0)
		{
			char nume[NAMESZ];
			char ip[NAMESZ];
			int port;
			double dif;
			sscanf(buffer, "%*s %s %d %lf %s", nume, &port, &dif, ip);
			cout << "Info despre " << nume << " cu portul " << port;
			cout << " conectat de " << dif << " secunde la ip-ul " << ip << endl;

			return;
		}

		// Am primit shared ok
		if (strcmp(comanda, "shared_OK") == 0)
		{
			cout << "client: Fișierul a fost share-uit cu succes\n";
			return;
		}

		// Am primit shared NOTOK
		if (strcmp(comanda, "shared_NOTOK") == 0)
		{
			char nume_fis[BUFLEN];
			sscanf(buffer, "%*s %s", nume_fis);
			cout << "client: Fișierul " << nume_fis << " este deja share-uit\n";
			return;
		}

		// Am primit getshare_NOTOK
		if (strcmp(comanda, "getshare_NOTOK") == 0)
		{
			char nume_client[NAMESZ];
			sscanf(buffer, "%*s %s", nume_client);
			cout << "client: Clientul " << nume_client << " nu se afla pe server\n";
			return;
		}

		// Am primit lista cu fisiere share-uite
		if (strcmp(comanda, "shared-files") == 0)
		{
			cout << "Lista de fisiere este:\n" << buffer + strlen("shared-files ") << endl;
		}
	}
}

// Trimite chunk de fisier
void send_file_piece(char *buffer, int cli_sock, ifstream& file)
{
	unsigned int i;
	bool found = false;
	for (i = 0; i < clienti.size(); ++i)
		if (clienti[i].sock == cli_sock)
		{
			found = true;
			cerr << "FOUNDDDD \n";
			break;
		}

	char bufbig[BUFFILE];
	memset(bufbig, 0, BUFFILE);

	if(found && clienti[i].size_fis)
	{
		file.read(bufbig, BUFFILE);
		clienti[i].size_fis -= file.gcount();
		cerr << "Remaining " << clienti[i].size_fis << " bytes to transfer\n";

		int n = send(cli_sock, bufbig, sizeof(bufbig), 0);
		send_verify(n);

		parse_recv_file(buffer, cli_sock);
	}
}

// Parsare mesaj venit de la un client
void parse_msg_client(char *buffer, int cli_sock)
{
	char comanda[CMDSZ];
	char nume[NAMESZ];
	sscanf(buffer, "%s %*s", comanda);
	cerr << "CMD-parse-msg-cli {" << comanda << "}\n";

	if (strcmp(comanda, "text-msg") == 0)
	{
		sscanf(buffer, "%*s %s", nume);
		cout << nume << ": " << buffer + strlen("text-msg ") + strlen(nume) + 1 << endl;
		return;
	}

	if (strcmp(comanda, "getfile") == 0)
	{
		char nume_client[NAMESZ];
		char fis[NAMESZ];
		sscanf(buffer, "getfile %s %s %s", nume_client, nume, fis);
		cerr << nume << ": " << nume_client << " mi-a cerut fisierul " << fis << endl;

		// Open file to be sent
		ifstream file;
		file.open(fis, ios::binary);
		if (!file.is_open())
		{
			cerr << "Fisierul " << fis << " nu poate fi deschis\n";
			cerr << "Trimit catre " << nume_client << " o instiintare\n";
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "fis-inexistent %s %s", nume_client, fis);
			cerr << "buff-err-fis {" << buffer << "}\n";

			int n = send(cli_sock, buffer, strlen(buffer), 0);
			send_verify(n);

			parse_cli_srv(buffer, cli_sock);
			return;
		}

		// Get the file size
		int size = 0;
		file.seekg(0, ios::end);
		size = file.tellg();
		file.seekg(0, ios::beg);


		memset(buffer, 0, BUFLEN);
		sprintf(buffer, "SIZE %d %s", size, fis);
		cerr << "TRIMIT-SIZE-fis+nume {" << buffer << "}\n";
		int n = send(cli_sock, buffer, strlen(buffer), 0);
		send_verify(n);

		cerr << "Am trimis nume + size fisier\n";
		if (!parsed_size)
		{
			//cerr << "Before parse_cli_srv\n";
			parse_cli_srv(buffer, cli_sock);
			//cerr << "After_parse_cli_srv\n";
			parsed_size = true;
		}
		unsigned int i;
		for (i = 0; i < clienti.size(); ++i)
			if (clienti[i].sock == cli_sock)
			{
				clienti[i].size_fis = size;
				cerr << "FOUNDDDD \n";
				break;
			}
		//cerr << "Before send_file\n";
		send_file_piece(buffer, cli_sock, file);
		//cerr << "After send_file\n";

		return;
	}
}

int main(int argc, char *argv[])
{
	int i, n, accept_len;
	struct sockaddr_in serv_addr, listen_addr, accept_addr;

	char buffer[BUFLEN];

	// Usage error
	if (argc < 4)
	{
		fprintf(stderr,"Usage %s client_name server_address server_port\n", argv[0]);
		exit(0);
	}

	// Socket pentru conectare la server
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error((char *)"ERROR opening socket at client");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	inet_aton(argv[2], &serv_addr.sin_addr);

	if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0)
		error((char *)"ERROR connecting to server");

		// Socket pentru ascultare conexiuni de la alți clienți
	listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sockfd < 0)
		error((char *)"ERROR opening listening socket at client");

	memset((char *) &listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(0);			// Port asignat automat
	listen_addr.sin_addr.s_addr = INADDR_ANY;	// Adresa IP a mașinii

	if (bind(listen_sockfd, (struct sockaddr *) &listen_addr, sizeof(struct sockaddr)) < 0)
		error((char *)"ERROR on binding client's listen_socket");

	listen(listen_sockfd, MAX_CLIENTS);

	// Aflu portul pe care asculta clientul
	socklen_t listen_len = sizeof(listen_addr);
	if (getsockname(listen_sockfd, (struct sockaddr *) &listen_addr, &listen_len) == -1)
		error((char *)"ERROR getting socket name");

//	fprintf(stderr, "%s's listen port number: %d\n", argv[1],ntohs(listen_addr.sin_port));

	// Handshake cu server-ul
	memset(buffer, 0, BUFLEN);
	// connect Nume_client listen_port_lcient
	sprintf(buffer,"connect %s %d", argv[1], ntohs(listen_addr.sin_port) );
	n = send(sockfd,buffer,strlen(buffer), 0);
	if (n < 0)
		 error((char *)"ERROR writing to socket");

	memset(buffer, 0, BUFLEN);
	if ((n = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0)
	{
		if (n == 0)
		{
			//conexiunea s-a inchis
			printf("client: socket with server hung up\n");
		} else
		{
			error((char *)"ERROR in recv");
		}
		close(sockfd);
		error((char *)"Problem in connection with server. Closing client...");
	}
	else
	{
		// Am primit instiintare de disconnect
		if (strncmp(buffer, "disconnected", strlen("disconnected")) == 0)
		{
			cout << "Client " << buffer << endl;
			close(sockfd);
			exit(0);
		}

		// Altfel am primit instiintare de connected
		cout << "Client " << buffer << " successfully to server\n";
	}

	//golim multimea de descriptori de citire (read_fds) si multimea (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	//adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(listen_sockfd, &read_fds);
	fdmax = listen_sockfd;

	//adaugam stdin in multimea read_fds
	FD_SET(fileno(stdin), &read_fds);

	//adaugam file descriptor-ul (socketul pe care e conectat clientul la server) in multimea read_fds
	FD_SET(sockfd, &read_fds);

	//main loop
	while(1)
	{
		tmp_fds = read_fds;

		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error((char *)"ERROR in select");

		for (i = 0; i <= fdmax; ++i)
		{
			if (FD_ISSET(i, &tmp_fds))
			{
				if (i == fileno(stdin) )
				{
					//citesc de la tastatura
					memset(buffer, 0 , BUFLEN);
					fgets(buffer, BUFLEN-1, stdin);

					if ( parse_command(buffer, argv[1]) == true )
						parse_message(sockfd);
				}

				else if (i == sockfd)
				{
					// Am primit ceva de la server
					memset(buffer, 0, BUFLEN);
					if ((n = recv(sockfd, buffer, sizeof(buffer), 0)) <= 0)
					{
						if (n == 0)
						{
							//conexiunea s-a inchis
							printf("client: socket with server hung up\n");
						} else
						{
							error((char *)"ERROR in recv");
						}
						close(sockfd);
						close(listen_sockfd);
						cerr << "Server has quit. Closing client...\n";
						exit(0);
					}
					else
					{
						// Am primit instiintare de disconnect
						if (strncmp(buffer, "disconnected", strlen("disconnected")) == 0)
						{
							cout << "Client " << buffer << endl;
							close(sockfd);
							exit(0);
						}

						// Altfel am primit instiintare de connected
						error((char *)"ERROR n-am primit ce trebuie de la server");
					}
				}

				else if (i == listen_sockfd)
				{
					// a venit ceva pe socketul de ascultare = o nouă conexiune
					// acțiune: accept()
					accept_len = sizeof(accept_addr);
					if ((newsockfd = accept(listen_sockfd, (struct sockaddr *)&accept_addr, (socklen_t *)&accept_len)) == -1)
					{
						error((char *)"ERROR in accept");
					}
					else
					{
						//adaug noul socket intors de accept() la multimea descriptorilor de citire
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax)
						{
							fdmax = newsockfd;
						}
					}
					fprintf(stderr, "Noua conexiune de la %s, port %d, socket_client %d\n", inet_ntoa(accept_addr.sin_addr), ntohs(accept_addr.sin_port), newsockfd);

				}

				else
				{
					// am primit date pe unul din socketii cu care vorbesc cu clientii
					//actiune: recv()
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0)
					{
						if (n == 0)
						{
							//conexiunea s-a inchis
							printf("client %s: socket %d hung up\n", argv[1], i);
						} else
						{
							error((char *)"ERROR in recv la clientul care așteapta mesaj");
						}
						close(i);
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care l-am folosit
					}

					else
					{ //recv intoarce >0
						fprintf (stderr, "Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);
						parse_msg_client(buffer, i);
					}
				}
			}
		}



	}

	close(listen_sockfd);
	close(sockfd);
	return 0;
}
