#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "helpers.h"
#include <deque>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>

#define DRUMAX 10000
#define MAX_LONG DRUMAX
#define INF DRUMAX

//#define DEBUG

using namespace std;

// Verifica daca exista un mesaj mai vechi in coada, si inlocuieste-l
// Altfel, daca nu exista, il baga in coada; Altfel, drop
void adauga_mesaj ( deque<msg>& coada, msg mesaj ) {
	for (unsigned int i = 0; i < coada.size(); ++i) {
		if (coada[i].creator == mesaj.creator && coada[i].type == mesaj.type &&
			coada[i].nr_secv == mesaj.nr_secv) {

			return;
		}
	}
	coada.push_back(mesaj);
}

// Afla vecinii unui nod
void get_vecini ( int topologie[KIDS][KIDS], int nod, vector<pair<int,int> >& vecini ) {
	for (int i = 0; i < KIDS; ++i) {
		#ifdef DEBUG
		cout << "topo: ["<<i<<"]["<<nod<<"] "<<topologie[i][nod]<<endl;
		#endif
		if ( (topologie[i][nod] != INF || topologie[nod][i] != INF) && i != nod) {
//			cout << "topo: ["<<i<<"]["<<nod<<"] "<<topologie[i][nod]<<endl;
			vecini.push_back(make_pair(i, topologie[i][nod]));
		}
	}
}

// Algoritm Dijkstra
void Dijkstra ( int topologie[KIDS][KIDS], int start, int (&drum)[KIDS], int (&parents)[KIDS] ) {
	int n = KIDS;	//numar de noduri
	for (int i = 0; i < KIDS; ++i)
		for (int j = i+1; j < KIDS; ++j)
			if (topologie[i][j] != INF)
				n++;
	
	set<pair<int,int> > Q;	// Set folosit drept coada
	drum[start] = 0;
	Q.insert(make_pair(start,0));
	
	while ( !Q.empty() ) {
		pair<int,int> top = *Q.begin();
		Q.erase(Q.begin());
		
		int v = top.first;		//nod
		
		vector<pair<int,int> > vecini;
		get_vecini( topologie, v, vecini);
		for (unsigned int i = 0; i < vecini.size(); ++i) {
			int v2 = vecini[i].first;
			int cost = vecini[i].second;
			
			if (drum[v2] > drum[v] + cost) {
				if(drum[v2] != INF) {
					Q.erase(Q.find(make_pair(v2, drum[v2])));
				}
				
				drum[v2] = drum[v] + cost;
				parents[v2] = v;
				Q.insert(make_pair(v2, drum[v2]));
			}
		}
	}
}

int main (int argc, char ** argv)
{

	int pipeout = atoi(argv[1]);
	int pipein = atoi(argv[2]);
	int nod_id = atoi(argv[3]); //procesul curent participa la simulare numai dupa ce nodul cu id-ul lui este adaugat in topologie
	int timp =-1 ;
	int gata = FALSE;
	msg mesaj;
	int cit, k;

	deque<msg> coada_old;		//coada mesaje vechi
	deque<msg> coada_new;		//coada mesaje noi
	msg LSADb[KIDS];
	int topologie[KIDS][KIDS];
	int secv = 1;			// Numar secventa unic per router
	
	// Initializare matrice topologie
	for (int i = 0; i < KIDS; ++i)
		for (int j = 0; j < KIDS; ++j)
			topologie[i][j] = INF;

	// Initializare LSA Database
	for (int i = 0; i < KIDS; ++i)
		LSADb[i].type = -1;

	//nu modificati numele, modalitatea de alocare si initializare a tabelei de rutare - se foloseste la mesajele de tip 8/10, deja implementate si la logare
	int tab_rutare [KIDS][2]; //tab_rutare[k][0] reprezinta costul drumului minim de la ruterul curent (nod_id) la ruterul k
								//tab_rutare[k][1] reprezinta next_hop pe drumul minim de la ruterul curent (nod_id) la ruterul k

	for (k = 0; k < KIDS; k++) {
		tab_rutare[k][0] = 10000;  // drum =DRUMAX daca ruterul k nu e in retea sau informatiile despre el nu au ajuns la ruterul curent
		tab_rutare[k][1] = -1; //in cadrul protocolului(pe care il veti implementa), next_hop =-1 inseamna ca ruterul k nu e (inca) cunoscut de ruterul nod_id (vezi mai sus)
	}

	printf ("Nod %d, pid %u alive & kicking\n", nod_id, getpid());

	if (nod_id == 0) { //sunt deja in topologie
		timp = -1; //la momentul 0 are loc primul eveniment
		mesaj.type = 5; //finish procesare mesaje timp -1
		mesaj.sender = nod_id;
		write (pipeout, &mesaj, sizeof(msg));
		printf ("TRIMIS Timp %d, Nod %d, msg tip 5 - terminare procesare mesaje vechi din coada\n", timp, nod_id);

	}

	while (!gata) {
		cit = read(pipein, &mesaj, sizeof(msg));

		if (cit <= 0) {
			printf ("Adio, lume cruda. Timp %d, Nod %d, msg tip %d cit %d\n", timp, nod_id, mesaj.type, cit);
			exit (-1);
		}

		#ifdef DEBUG
		// Debug in 10 fisiere
		for (int x = 0; x < KIDS; ++x) {
			fstream f;
			char str[20];
			sprintf(str, "debug%d.txt", x);
			f.open (str, fstream::in | fstream::out | fstream::app);
			
			if (nod_id == x) {
				f << "Timp " << timp << " afisez topologia nodului " << nod_id << endl;
				for (int i = 0; i < KIDS; ++i) {
					for (int j = 0; j < KIDS; ++j)
						f << topologie[i][j] << " ";
					f << endl;
				}
			}
			f.close();
		}
		#endif

		switch (mesaj.type) {

			//1,2,3,4 sunt mesaje din protocolul link state;
			//actiunea imediata corecta la primirea unui pachet de tip 1,2,3,4 este buffer-area (punerea in coada /coada new daca sunt 2 cozi - vezi enunt)
			//mesajele din coada new se vor procesa atunci cand ea devine coada old (cand am intrat in urmatorul pas de timp)
			case 1:
				printf ("Timp %d, Nod %d, msg tip 1 - LSA\n", timp, nod_id);
				adauga_mesaj ( coada_new, mesaj );
				break;

			case 2:
				printf ("Timp %d, Nod %d, msg tip 2 - Database Request\n", timp, nod_id);
				adauga_mesaj ( coada_new, mesaj );
				break;

			case 3:
				printf ("Timp %d, Nod %d, msg tip 3 - Database Reply\n", timp, nod_id);
				adauga_mesaj ( coada_new, mesaj );
				break;

			case 4:
				printf ("Timp %d, Nod %d, msg tip 4 - pachet de date (de rutat)\n", timp, nod_id);
				adauga_mesaj ( coada_new, mesaj );
				break;

			case 6://complet in ceea ce priveste partea cu mesajele de control
					//puteti inlocui conditia de coada goala, ca sa corespunda cu implementarea personala
					//aveti de implementat procesarea mesajelor ce tin de protocolul de rutare
				{
				timp++;
				printf ("Timp %d, Nod %d, msg tip 6 - incepe procesarea mesajelor puse din coada la timpul anterior (%d)\n", timp, nod_id, timp-1);

				//veti modifica ce e mai jos -> in scheletul de cod nu exista nicio coada
//				int coada_old_goala = TRUE;

				//daca NU mai am de procesat mesaje venite la timpul anterior
				//(dar mai pot fi mesaje venite in acest moment de timp, pe care le procesez la t+1)
				//trimit mesaj terminare procesare pentru acest pas (tip 5)
				//altfel, procesez mesajele venite la timpul anterior si apoi trimit mesaj de tip 5
//				while (!coada_old_goala) {
				
				coada_old = coada_new;
				coada_new.clear();
				
				while ( !coada_old.empty() ) {
					//	procesez tote mesajele din coada old
					//	(sau toate mesajele primite inainte de inceperea timpului curent - marcata de mesaj de tip 6)
					//	la acest pas/timp NU se vor procesa mesaje venite DUPA inceperea timpului curent
//cand trimiteti mesaje de tip 4 nu uitati sa setati (inclusiv) campurile, necesare pt logare:  mesaj.timp, mesaj.creator, mesaj.nr_secv, mesaj.sender, mesaj.next_hop
					//la tip 4 - creator este sursa initiala a pachetului rutat

					msg temp = coada_old.front();
					coada_old.pop_front();
					switch ( temp.type ) {
						case 1:
						case 3:
							{
							printf("Timp %d Procesez msg type 1-3 creator %d \n", timp, temp.creator);
							if ( LSADb[temp.creator].nr_secv < temp.nr_secv ) {
								
								
								stringstream ss (stringstream::in | stringstream::out);
								ss << temp.payload;
								int size, timp_creare, vec, cost;
								ss >> size;
								
								vector<pair<int,int> > vecini;
								for (int i = 0; i < size; ++i) {
									ss >> vec >> cost;
									vecini.push_back(make_pair(vec,cost));
								}
								ss >> timp_creare;
								
								stringstream ssold (stringstream::in | stringstream::out);
								int size_old, timp_creare_old, vec_old, cost_old;
								ssold << LSADb[temp.creator].payload;
								
								ssold >> size_old;
								vector<pair<int,int> > vecini_old;
								for (int i = 0; i < size; ++i) {
									ssold >> vec_old >> cost_old;
								}
								ssold >> timp_creare_old;
								
								#ifdef DEBUG
								cout << "~~~~~~~~~Timp curent "<<timp << " timp creare old " <<timp_creare_old<< " t_cr "<<timp_creare<<endl;
								#endif
								
								
								// Daca timpul de creare al LSA-ului primit este mai vechi, il ignor
								if ( timp_creare < timp_creare_old )
									break;
								
								// Updatez LSADatabase
								LSADb[temp.creator] = temp;
								
								// Updatez topologia pentru acel creator
								for (int i = 0; i < KIDS; ++i) {
									topologie[temp.creator][i] = INF;
									topologie[i][temp.creator] = INF;
								}
								for (unsigned int i = 0; i < vecini.size(); ++i) {
									topologie[vecini[i].first][temp.creator] = vecini[i].second;
									topologie[temp.creator][vecini[i].first] = vecini[i].second;
								}
								
								// Daca am mesaj de tip 3 nu mai forwardez
								if (temp.type == 3)
									break;
							
								
								// Forwardez LSA
								temp.sender = nod_id;
								temp.timp = timp;
								for (unsigned int i = 0; i < vecini.size(); ++i) {
									temp.next_hop = vecini[i].first;
									
									write(pipeout, &temp, sizeof(msg));
								}
								
								
								#ifdef DEBUG
								cout << "~~~~~~~~Test: nr-vecini " << size << " timp creare " << timp_creare;
								#endif
								for (int i = 0; i < size; ++i)
									cout << " vec " << vecini[i].first << " cost "<<vecini[i].second << "\n";
							
							}
						}
							break;
						case 2:
							{
							printf("Procesez msg type 2 creator %d \n", temp.creator);
							
							
							// Trimit Database Reply
							for (int i = 0; i < KIDS; ++i) {
								if (LSADb[i].type == -1 || nod_id == i)
									continue;
								
								msg tosend;
								tosend = LSADb[i];
								tosend.creator = i;
								tosend.type = 3;
								tosend.sender = nod_id;
								tosend.timp = timp;
								tosend.next_hop = temp.creator;
								
								#ifdef DEBUG
								cout << "Timp "<<timp <<"LSA-payload trimis de catre " << nod_id << " lui " << i << "{";
								cout << tosend.payload << "}\n";
								#endif
								
								write(pipeout, &tosend, sizeof(msg));
							}
							
							// Creez un LSA propriu si updatez LSADatabase
							
							msg lsa;
							lsa.type = 1;
							lsa.creator = nod_id;
							lsa.sender = nod_id;
							lsa.timp = timp;
							lsa.nr_secv = secv++;
							
							int cost;
							stringstream ss (stringstream::in | stringstream::out);
							ss << temp.payload;
							ss >> cost;
							#ifdef DEBUG
							cout << ">>>>>>>>>>>nod_id " << nod_id << " creator " << temp.creator << " cost " << cost << "\n";
							#endif
							
							vector<pair<int,int> > vecini;
							get_vecini( topologie, nod_id, vecini );
							bool gasit = false;
							for (unsigned int i = 0; i < vecini.size(); ++i) {
								if ( !gasit && vecini[i].first == temp.creator ) {
									gasit = true;
									break;
								}
							}
							if (!gasit)
								vecini.push_back(make_pair(temp.creator, cost));
							
							// Creez payload pentru un nou LSA
							stringstream ssout (stringstream::in | stringstream::out);
							ssout << vecini.size() << " ";
							for (unsigned int i = 0; i < vecini.size(); ++i) {
								ssout << vecini[i].first << " " << vecini[i].second << " ";
							}
							ssout << timp;		//timpul de creare al lsa-ului
							
//							cout << "SSSOUUUUTTTT {" << ssout.str() << "}\n";
							
							
							string sir = ssout.str();
							#ifdef DEBUG
							cout << "SIR {" << sir << "}\n";
							#endif
							for (unsigned int i = 0; i < sir.size(); ++i)
								lsa.payload[i] = sir[i];
							lsa.payload[sir.size()] = '\0';
							
							#ifdef DEBUG
							cout << "AM CREAT LSA: nod_id " << nod_id << " payload{" << lsa.payload << "}\n";
							#endif
							
							// Updatez lsadatabase
							LSADb[nod_id] = lsa;
							
							// Updatez topologia
							topologie[nod_id][temp.creator] = cost;
							topologie[temp.creator][nod_id] = cost;
							
							// Trimit LSA catre toti vecinii
							for (unsigned int i = 0; i < vecini.size(); ++i) {
								lsa.next_hop = vecini[i].first;
								
								write(pipeout, &lsa, sizeof(msg));
							}
							
							}
							break;
						case 4:
							{
								int dest;
								sscanf(temp.payload, "%d", &dest);
								
								if (tab_rutare[dest][1] != -1) {
									msg pack = temp;
									pack.sender = nod_id;
									pack.timp = timp;
									pack.next_hop = tab_rutare[dest][1];
								
									write(pipeout, &pack, sizeof(msg));
								}
							}
							break;
						default:
							break;
					}
				}

				int drum[KIDS], parents[KIDS];
				for (int i = 0; i < KIDS; ++i) {
					drum[i] = INF;
					parents[i] = -1;
				}
				
				Dijkstra(topologie, nod_id, drum, parents );
				for (int i = 0; i < KIDS; ++i) {
					
					int u = i, prev = -1;
					while (parents[u] != -1) {
						prev = u;
						u = parents[u];
					}
					tab_rutare[i][0] = drum[i];
					if (prev != -1)
						tab_rutare[i][1] = prev;
				}
				

				//acum coada_old e goala, trimit mesaj de tip 5
					mesaj.type = 5;
					mesaj.sender = nod_id;
					write (pipeout, &mesaj, sizeof(msg));
				}
				break;

			case 7: //complet in ceea ce priveste partea cu mesajele de control
					//aveti de implementat tratarea evenimentelor si trimiterea mesajelor ce tin de protocolul de rutare
					//in campul payload al mesajului de tip 7 e linia de fisier (%s) corespunzatoare respectivului eveniment
					//vezi multiproc.c, liniile 88-115 (trimitere mes tip 7) si liniile 184-194 (parsare fisiere evenimente)

					//rutere direct implicate in evenimente, care vor primi mesaje de tip 7 de la simulatorul central:
					//eveniment tip 1: ruterul nou adaugat la retea  (ev.d1  - vezi liniile indicate)
					//eveniment tip 2: capetele noului link (ev.d1 si ev.d2)
					//eveniment tip 3: capetele linkului suprimat (ev.d1 si ev.d2)
					//evenimet tip 4:  ruterul sursa al pachetului (ev.d1)
				{
				if (mesaj.join == TRUE) {
					timp = mesaj.timp;
					printf ("Nod %d, msg tip eveniment - voi adera la topologie la pasul %d\n", nod_id, timp+1);
					
					stringstream ss (stringstream::in | stringstream::out);
					ss << mesaj.payload;
					int ev, nr_vec, id_rut_nou;
					int vec, cost;
					ss >> ev >> id_rut_nou >> nr_vec;
					
					#ifdef DEBUG
					cout << "-----Ev " << ev << " id_rut_nou " << id_rut_nou << " nr_vec " << nr_vec << endl;
					#endif
					
					msg newmsg;
					newmsg.type = 2;
					newmsg.sender = nod_id;
					newmsg.timp = timp;
					newmsg.creator = nod_id;
					newmsg.nr_secv = secv++;
					for (int i = 0; i < nr_vec; ++i){
						ss >> vec >> cost;
						cout << "---Vecin " << vec << " cu cost " << cost << endl;
						
						stringstream ssout (stringstream::in | stringstream::out);
						ssout << cost;
						
						// Trimit Database Request catre vecini
						newmsg.next_hop = vec;
						ssout >> newmsg.payload;
						
						write (pipeout, &newmsg, sizeof(msg));
					}
				}
				else if (mesaj.payload[0] == '2') {
					timp = mesaj.timp;
					printf ("Nod %d, msg tip eveniment - voi auga link la pasul %d\n", nod_id, timp+1);
					
					stringstream ss (stringstream::in | stringstream::out);
					ss << mesaj.payload;
					int ev, rut1, rut2, cost;
					ss >> ev >> rut1 >> rut2 >> cost;
					
					stringstream ssout (stringstream::in | stringstream::out);
					ssout << cost;
					
					// Trimit Database Request catre celalalt capat
					msg newmsg;
					newmsg.type = 2;
					newmsg.timp = timp;
					newmsg.nr_secv = secv++;
					newmsg.creator = nod_id;
					newmsg.sender= nod_id;
					ssout >> newmsg.payload;
					
					if (rut2 == nod_id)
						rut2 = rut1;
					
					newmsg.next_hop = rut2;
					cout << "<<<<Trimit dbreq de la " << nod_id << " catre " << rut2 << endl;
					write (pipeout, &newmsg, sizeof(msg));
				}
				else if (mesaj.payload[0] == '3') {
					timp = mesaj.timp;
					printf ("Nod %d, msg tip eveniment - voi rupe link la pasul %d\n", nod_id, timp+1);
					
					stringstream ss (stringstream::in | stringstream::out);
					ss << mesaj.payload;
					int ev, rut1, rut2;
					ss >> ev >> rut1 >> rut2;
					
					if (nod_id == rut2)
						rut2 = rut1;
					
					topologie[nod_id][rut2] = INF;
					topologie[rut2][nod_id] = INF;
					
					vector<pair<int,int> > vecini;
					get_vecini ( topologie, nod_id, vecini );
					
					// Introduc lista de vecini+costuri in ssout
					// Structura payload:::: 1) nr_vecini; 2) vecin cost; 3) timp_creare
					stringstream ssout (stringstream::in | stringstream::out);
					ssout << vecini.size() << " ";
					for (unsigned int i = 0; i < vecini.size(); ++i)
						ssout << vecini[i].first << " " << vecini[i].second << " ";
					
					// Atasez si timpul de creare
					#ifdef DEBUG
					cout << "Timp " << timp << " ";
					#endif
					
					ssout << timp;
					
					#ifdef DEBUG
					cout << "Nod " << nod_id <<" vecini costuri : {"<<ssout.str()<<"}\n";
					#endif
					
					// Creez LSA-ul
					msg lsa;
					lsa.type = 1;
					lsa.creator = nod_id;
					lsa.timp = timp;
					lsa.sender = nod_id;
					lsa.nr_secv = secv++;

					string sir = ssout.str();
					for (unsigned int i = 0; i < sir.size(); ++i)
						lsa.payload[i] = sir[i];
					lsa.payload[sir.size()] = '\0';
					
					#ifdef DEBUG
					cout << "Nod " << nod_id <<" vecini costuri : {"<<lsa.payload<<"}\n";
					#endif
					
					// Updatez LSADatabase
					LSADb[nod_id] = lsa;
					
					// Trimit LSA-ul
					for (unsigned int i = 0; i < vecini.size(); ++i) {
						lsa.next_hop = vecini[i].first;
						
						write(pipeout, &lsa, sizeof(msg));
					}
					
					// Updatez tabela de rutare
					int drum[KIDS], parents[KIDS];
					for (int i = 0; i < KIDS; ++i) {
						drum[i] = INF;
						parents[i] = -1;
					}
				
					Dijkstra(topologie, nod_id, drum, parents );
					for (int i = 0; i < KIDS; ++i) {
					
						int u = i, prev = -1;
						while (parents[u] != -1) {
							prev = u;
							u = parents[u];
						}
						tab_rutare[i][0] = drum[i];
						if (prev != -1)
							tab_rutare[i][1] = prev;
					}
				
				}
				else if (mesaj.payload[0] == '4') {
					int ev, rut1, rut2;
					sscanf(mesaj.payload, "%d %d %d", &ev, &rut1, &rut2);
					
					if (nod_id == rut1 && tab_rutare[rut2][1] != -1) {
						msg pack;
						pack.type = 4;
						pack.creator = nod_id;
						pack.sender = nod_id;
						pack.nr_secv = secv++;
						pack.timp = timp;
						pack.len = timp;	//timp creare
						pack.next_hop = tab_rutare[rut2][1];
						sprintf(mesaj.payload, "%d", rut2);
					
						write(pipeout, &pack, sizeof(msg));
					}
				}
				else
					printf ("Timp %d, Nod %d, msg tip 7 - eveniment\n", timp+1, nod_id);
				//acest tip de mesaj (7) se proceseaza imediat - nu se pune in nicio coada (vezi enunt)
				}
				break;

			case 8: //complet implementat - nu modificati! (exceptie afisari on/off)
				{
				//printf ("Timp %d, Nod %d, msg tip 8 - cerere tabela de rutare\n", timp+1, nod_id);
				mesaj.type = 10;  //trimitere tabela de rutare
				mesaj.sender = nod_id;
				memcpy (mesaj.payload, &tab_rutare, sizeof (tab_rutare));
				//Observati ca acest tip de mesaj (8) se proceseaza imediat - nu se pune in nicio coada (vezi enunt)
				write (pipeout, &mesaj, sizeof(msg));
				}
				break;

			case 9: //complet implementat - nu modificati! (exceptie afisari on/off)
				{
				//Aici poate sa apara timp -1 la unele "noduri"
				//E ok, e vorba de procesele care nu reprezentau rutere in retea, deci nu au de unde sa ia valoarea corecta de timp
				//Alternativa ar fi fost ca procesele neparticipante la simularea propriu-zisa sa ramana blocate intr-un apel de read()
				printf ("Timp %d, Nod %d, msg tip 9 - terminare simulare\n", timp, nod_id);
				gata = TRUE;
				}
				break;


			default:
				printf ("\nEROARE: Timp %d, Nod %d, msg tip %d - NU PROCESEZ ACEST TIP DE MESAJ\n", timp, nod_id, mesaj.type);
				exit (-1);
		}
	}

return 0;

}
