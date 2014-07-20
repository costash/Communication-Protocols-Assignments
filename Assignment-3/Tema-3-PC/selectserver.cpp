// Constantin Serban-Radoi 323CA
// Tema 3 PC
// Aprilie 2012

#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <ctime>

#define MAX_CLIENTS 5
#define BUFLEN 256
#define NAMESZ 100
#define CMDSZ 100

using namespace std;

typedef struct client_
{
	string nume;	// Nume client
	int port;		// Port de ascultare
	struct sockaddr_in adresa;	// Adresa clientului
	int srv_sockfd;		// Socket pe care e conectat la server
	time_t timp_conectare;	// Timp conectare la server
	vector<string> shared_files;	// Fisiere partajate
}client;	// Structura cu informatii despre conexiunea cu un client

vector<client> clienti;	// Vector cu toti clientii conectati

int sockfd, newsockfd;
int fdmax;		//valoare maxima file descriptor din multimea read_fds

fd_set read_fds;	//multimea de citire folosita in select()
fd_set tmp_fds;	//multime folosita temporar

//vector<string> clienti;

void error(char *msg)
{
	perror(msg);
	exit(1);
}

// Parsare comanda de la tastatura
void parse_command(char *buffer)
{
	if (strcmp(buffer, "status\n") == 0)
	{
		fprintf(stdout, "Am primit comanda status\nAfișez informații despre clienți...\n");

		for (unsigned int i = 0; i < clienti.size(); ++i)
		{
			time_t current_time;
			time (&current_time);
			double dif = difftime(current_time, clienti[i].timp_conectare);

			cout << clienti[i].nume << " conectat de " << dif << " secunde de pe ip-ul ";
			cout << inet_ntoa(clienti[i].adresa.sin_addr) << " si portul " << clienti[i].port;
			cout << "\nLista de fisiere share-uite a sa este:\n{";
			for (unsigned int j = 0; j < clienti[i].shared_files.size(); ++j)
				cout << " " << clienti[i].shared_files[j];
			cout << " }\n";
		}

		return;
	}
	if (strcmp(buffer, "quit\n") == 0)
	{
		fprintf(stderr, "Am primit comanda quit și închid server-ul\n");

		for (int i = 0; i <= fdmax; ++i)
			if ( i != fileno(stdin) && FD_ISSET(i, &tmp_fds))
			{
				memset(buffer, 0, BUFLEN);
				sprintf(buffer, "disconnected a fost inchis serverul");

				int n = send(i, buffer, strlen(buffer), 0);
				if (n < 0)
					 error((char *)"ERROR writing to socket at disconnect");

				close(i);
				FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul
			}
		close(sockfd);

		exit(0);
	}

	fprintf(stderr, "Wrong command. Usage: \"status\" or \"quit\"\n");
	return;
}

// Parsare mesaj de la client
void parse_message(char *buffer, char comanda[CMDSZ], int sock, struct sockaddr_in adresa)
{
	char nume[NAMESZ];

	int port;
	client cl;

	if ( strcmp(comanda, "connect") == 0)
	{
		cerr << "CMD {" << comanda << "}\n";

		sscanf(buffer, "%*s %s %d", nume, &port);
		cerr << "Nume: " << nume << " port: " << port << endl;

		string name(nume);
		//clienti.push_back(name);
		cl.nume = name;
		cl.port = port;
		cl.adresa = adresa;
		cl.srv_sockfd = sock;
		cl.shared_files.clear();
		time (&cl.timp_conectare);

		for (unsigned int i = 0; i < clienti.size(); ++i)
		{
			if (clienti[i].nume.compare(name) == 0)
			{
				cerr << "Un client cu numele " << name << " mai exista deja pe server\nSending disconnect notification...\n";

				// Anunt clientul ca va fi deconectat
				memset(buffer, 0, BUFLEN);
				sprintf(buffer, "disconnected mai exista un client cu acest nume");

				int n = send(sock, buffer, strlen(buffer), 0);
				if (n < 0)
					 error((char *)"ERROR writing to socket at disconnect");

				close(sock);
				FD_CLR(sock, &read_fds); // scoatem din multimea de citire socketul
				return;
			}
		}

		// Daca pastrez legatura il instiintez pe client
		memset(buffer, 0, BUFLEN);
		sprintf(buffer, "connected");

//		cerr << "SOCKFD between server and client: " << sock << endl;
		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at connected");

		clienti.push_back(cl);

//		cerr << "Client " << cl.nume << " port: " << cl.port << " creat la: " << ctime(&cl.timp_conectare) << " pe socketul " << cl.srv_sockfd ;
//		cerr << " si adresa ip: " << inet_ntoa(cl.adresa.sin_addr) << endl;

		cerr << "CLIENTI: ";
		for (unsigned int i = 0; i < clienti.size(); ++i)
			cerr << clienti[i].nume << " ";
		cerr << endl;
		return;
	}

	if ( strcmp(comanda, "listclients") == 0 )
	{
		cerr << "CMD {" << comanda << "}\n";

		sscanf(buffer, "%*s %s", nume);
		cerr << "nume: " << nume << endl;

		memset(buffer, 0, BUFLEN);
		cerr << "CLIENTI: ";
		for (unsigned int i = 0; i < clienti.size(); ++i)
			cerr << clienti[i].nume << " ";
		cerr << endl;

		stringstream ss (stringstream::in | stringstream::out);
		ss << "clienti";
		for (unsigned int i = 0; i < clienti.size(); ++i)
			ss << " " << clienti[i].nume;
		string buf;
		getline (ss, buf);
		buf.copy(buffer, buf.length());

		cerr << "BUFFER-CLIENTI: " << buffer << endl;
		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at listclients");

		return;
	}

	if ( strcmp(comanda, "infoclient") == 0)
	{
		cerr << "CMD {" << comanda << "}\n";

		char nume_client[NAMESZ];

		sscanf(buffer, "%*s %s %s", nume, nume_client);
		cerr << "client_care_cere_info: " << nume << " nume_client: " << nume_client << endl;

		unsigned int i;
		bool found = false;
		for (i = 0; i < clienti.size(); ++i)
			if (strcmp(nume_client, clienti[i].nume.c_str()) == 0)
			{
				found = true;
				break;
			}

//		cerr << "CLIENT_DESPRE_CARE CER INFO " << clienti[i].nume << endl;

		if (!found)
		{
			// Trimit mesaj inapoi ca nu exista clientul

			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "client-inexistent");

			int n = send(sock, buffer, strlen(buffer), 0);
			if (n < 0)
				 error((char *)"ERROR writing to socket at infoclient client-inexistent");

			return;
		}

		// Trimit informatiile cerute
		// "info-client nume_client port timp_de_la_conectare ip"
		memset(buffer, 0, BUFLEN);
		time_t current_time;
		time (&current_time);

		double dif = difftime(current_time, clienti[i].timp_conectare);
		cerr << "Este conectat de " << dif << " secunde\n";
		sprintf(buffer, "info-client %s %d %lf %s", clienti[i].nume.c_str(), clienti[i].port, dif, inet_ntoa(clienti[i].adresa.sin_addr));

		cerr << "BUFFER-info-cl {" << buffer << "}\n";

		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at infoclient");

		return;
	}

	if ( strcmp(comanda, "message") == 0)
	{
		cerr << "CMD {" << comanda << "}\n";
		char nume_client[BUFLEN];

		sscanf(buffer, "%*s %s %s", nume, nume_client);
		cerr << "Client-care-trimite-msg: " << nume << " catre clientul " << nume_client << endl;

		unsigned int i;
		bool found = false;
		for (i = 0; i < clienti.size(); ++i)
		{
			if ( strcmp(clienti[i].nume.c_str(), nume_client) == 0 )
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			// Clientul cautat nu exista
			cerr << "Clientul catre care se trimite mesaj nu exista\n";
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "info-message-NOTOK %s nu exista pe server", nume_client);
			cerr << "Trimit inapoi la client {" << buffer << "}\n";

			int n = send(sock, buffer, strlen(buffer), 0);
			if (n < 0)
				 error((char *)"ERROR writing to socket at message");

			return;
		}

		// Clientul cautat exista, trimit inapoi informatiile de conectare la el
		// Mesaj de forma: "info-message nume_client_care_trimite_msg nume_client_la_care_se_trimite ip port"
		memset(buffer, 0, BUFLEN);
		sprintf(buffer, "info-message %s %s %s %d", nume, nume_client, inet_ntoa(clienti[i].adresa.sin_addr), clienti[i].port);
		cerr << "Trimit inapoi la client {" << buffer << "}\n";

		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at message");

		return;
	}

	if ( strcmp(comanda, "sharefile") == 0 )
	{
		cerr << "CMD {" << comanda << "}\n";
		char nume_fisier[BUFLEN];

		sscanf(buffer, "%*s %s %s", nume, nume_fisier);
		cerr << "Client-care-share-uie: " << nume << " fisierul " << nume_fisier << endl;

		unsigned int i;
		bool found = false;
		for (i = 0; i < clienti.size(); ++i)
		{
			if ( strcmp(clienti[i].nume.c_str(), nume) == 0 )
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			error((char *)"Clientul care share-uie nu exista");
		}
		string nume_fis = nume_fisier;

		bool found_file = false;
		for (unsigned int j = 0; j < clienti[i].shared_files.size(); ++j)
			if (clienti[i].shared_files[j].compare(nume_fis) == 0)
			{
				found_file = true;
				break;
			}

		if (found_file)
		{
			cerr << "Fisierul " << nume_fis << " este deja share-uit\n";
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "shared_NOTOK %s", nume_fis.c_str());
			int n = send(sock, buffer, strlen(buffer), 0);
			if (n < 0)
				 error((char *)"ERROR writing to socket at sharefile");

			return;
		}

		clienti[i].shared_files.push_back(nume_fis);

		cerr << "Shared files:";
		for (unsigned int j = 0; j < clienti[i].shared_files.size(); ++j)
			cerr << " " << clienti[i].shared_files[j];
		cerr << endl;

		// Trimit mesaj inapoi la client ca fisierul s-a share-uit OK
		memset(buffer, 0, BUFLEN);
		sprintf(buffer, "shared_OK");
		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at sharefile");

		return;
	}

	if ( strcmp(comanda, "unsharefile") == 0 )
	{
		cerr << "CMD {" << comanda << "}\n";
		char nume_fisier[BUFLEN];

		sscanf(buffer, "%*s %s %s", nume, nume_fisier);
		cerr << "Client-care-unshare-uie: " << nume << " fisierul " << nume_fisier << endl;

		unsigned int i;
		bool found = false;
		for (i = 0; i < clienti.size(); ++i)
		{
			if ( strcmp(clienti[i].nume.c_str(), nume) == 0 )
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			error((char *)"Clientul care unshare-uie nu exista");
		}
		string nume_fis = nume_fisier;

		bool found_file = false;
		unsigned int j;
		for (j = 0; j < clienti[i].shared_files.size(); ++j)
			if (clienti[i].shared_files[j].compare(nume_fis) == 0)
			{
				found_file = true;
				break;
			}

		if (!found_file)
		{
			cerr << "Fisierul " << nume_fis << " nu exista la share\n";
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "unshared_NOTOK %s", nume_fis.c_str());
			int n = send(sock, buffer, strlen(buffer), 0);
			if (n < 0)
				 error((char *)"ERROR writing to socket at unsharefile");

			return;
		}

		clienti[i].shared_files.erase(clienti[i].shared_files.begin() + j);

		cerr << "Shared files: ";
		for (j = 0; j < clienti[i].shared_files.size(); ++j)
			cerr << clienti[i].shared_files[j];
		cerr << endl;

		// Trimit mesaj inapoi la client ca fisierul s-a share-uit OK
		memset(buffer, 0, BUFLEN);
		sprintf(buffer, "unshared_OK");
		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at unsharefile");

		return;
	}

	if ( strcmp(comanda, "getshare") == 0)
	{
		cerr << "CMD {" << comanda << "}\n";
		char nume_client[BUFLEN];

		sscanf(buffer, "%*s %s %s", nume, nume_client);
		cerr << "Client-care-cere-getshare: " << nume << " catre " << nume_client << endl;

		unsigned int i;
		bool found = false;
		for (i = 0; i < clienti.size(); ++i)
		{
			if ( strcmp(clienti[i].nume.c_str(), nume_client) == 0 )
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			cerr << "Clientul interogat nu exista\n";
			// Trimit mesaj inapoi la client mesaj ca nu exista clientul interogat
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "getshare_NOTOK %s", nume_client);
			int n = send(sock, buffer, strlen(buffer), 0);
			if (n < 0)
				 error((char *)"ERROR writing to socket at getshare");

			return;
		}

		stringstream ss (stringstream::in | stringstream::out);
		ss << "shared-files";
		for (unsigned int j = 0; j < clienti[i].shared_files.size(); ++j)
			ss << " " << clienti[i].shared_files[j];

		cerr << "BUFBUFBUF: " << ss.str() << endl;
		string buf;
		getline (ss, buf);
		memset(buffer, 0, BUFLEN);
		buf.copy(buffer, buf.length());

		cerr << "BUFFER-SHARED-FILES: " << buffer << endl;
		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at getshare");

		return;
	}

	if ( strcmp(comanda, "getfile") == 0 )
	{
		cerr << "CMD {" << comanda << "}\n";
		char nume_client[NAMESZ];
		char nume_fisier[NAMESZ];

		sscanf(buffer, "%*s %s %s %s", nume, nume_client, nume_fisier);
		cerr << nume << " vrea fisierul " << nume_fisier << " de la clientul " << nume_client << endl;

		unsigned int i;
		bool found = false;
		bool foundfile = false;
		for (i = 0; i < clienti.size(); ++i)
		{
			if ( strcmp(clienti[i].nume.c_str(), nume_client) == 0 )
			{
				found = true;
				break;
			}
		}
		unsigned int j;

		if (found)
			for (j = 0; j < clienti[i].shared_files.size(); ++j)
				if ( strcmp(clienti[i].shared_files[j].c_str(), nume_fisier) == 0)
				{
					foundfile = true;
					break;
				}

		if (!foundfile)
		{
			cerr << "Fisierul " << nume_fisier << " nu poate fi gasit\n";
			// Trimit mesaj inapoi la client mesaj ca nu exista fisierul interogat
			memset(buffer, 0, BUFLEN);
			sprintf(buffer, "getfile_NOTOK %s %s", nume_client, nume_fisier);
			int n = send(sock, buffer, strlen(buffer), 0);
			if (n < 0)
				 error((char *)"ERROR writing to socket at getshare");

			return;
		}
		// Fisierul cautat exista, trimit inapoi informatiile de conectare la client
		// Mesaj de forma: "info-message nume_client_care_trimite_msg nume_client_la_care_se_trimite fisier ip port"
		memset(buffer, 0, BUFLEN);
		sprintf(buffer, "info-getfile %s %s %s %s %d", nume, nume_client, nume_fisier, inet_ntoa(clienti[i].adresa.sin_addr), clienti[i].port);
		cerr << "Trimit inapoi la client {" << buffer << "}\n";

		int n = send(sock, buffer, strlen(buffer), 0);
		if (n < 0)
			 error((char *)"ERROR writing to socket at message");

		return;
	}


}

int main(int argc, char *argv[])
{
	int portno, clilen;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i;

	if (argc < 2)
	{
		fprintf(stderr,"Usage : %s port\n", argv[0]);
		exit(1);
	}

	//golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error((char *)"ERROR opening socket at server");

	portno = atoi(argv[1]);

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		error((char *)"ERROR on binding");

	listen(sockfd, MAX_CLIENTS);

	//adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	//adaugam stdin in multimea read_fds
	FD_SET(fileno(stdin), &read_fds);

	// main loop
	while (1)
	{

		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error((char *)"ERROR in select");

		for(i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &tmp_fds))
			{

				if (i == fileno(stdin))
				{
					// citesc de la tastatură o comandă
					memset(buffer, 0 , BUFLEN);
					fgets(buffer, BUFLEN-1, stdin);

					parse_command(buffer);
				}

				else if (i == sockfd)
				{
					// a venit ceva pe socketul de ascultare = o nouă conexiune
					// actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen)) == -1)
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
					fprintf(stderr, "Noua conexiune de la %s, port %d, socket_client %d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				}

				else
				{
					// am primit date pe unul din socketii cu care vorbesc cu clientii
					//actiunea serverului: recv()
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0)
					{
						if (n == 0)
						{
							//conexiunea s-a inchis
							//printf("selectserver: socket %d hung up\n", i);
							for (unsigned int j = 0; j < clienti.size(); ++j)
								if (clienti[j].srv_sockfd == i)
									printf("selectserver: clientul %s pe socket %d a iesit\n", clienti[j].nume.c_str(), i);
						} else
						{
							error((char *)"ERROR in recv");
						}
						for (unsigned int j = 0; j < clienti.size(); ++j)
						{
							if (clienti[j].srv_sockfd == i)
								clienti.erase(clienti.begin() + j);
						}
						close(i);
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul
					}

					else
					{ //recv intoarce >0
						fprintf (stderr, "Am primit de la clientul de pe socketul %d, mesajul: %s\n", i, buffer);

						char comanda[CMDSZ];
						sscanf(buffer,"%s %*s", comanda);
						cerr << "COMANDA: " << comanda << endl;

						parse_message(buffer, comanda, i, cli_addr);
					}
				}
			}
		}
	}


	close(sockfd);

	return 0;
}
