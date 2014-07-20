#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "helpers.h"

#define DRUMAX 10000

using namespace std;

int main (int argc, char ** argv)
{

	int pipeout = atoi(argv[1]);
	int pipein = atoi(argv[2]);
	int nod_id = atoi(argv[3]); //procesul curent participa la simulare numai dupa ce nodul cu id-ul lui este adaugat in topologie
	int timp =-1 ;
	int gata = FALSE;
	msg mesaj;
	int cit, k;

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

		switch (mesaj.type) {

			//1,2,3,4 sunt mesaje din protocolul link state;
			//actiunea imediata corecta la primirea unui pachet de tip 1,2,3,4 este buffer-area (punerea in coada /coada new daca sunt 2 cozi - vezi enunt)
			//mesajele din coada new se vor procesa atunci cand ea devine coada old (cand am intrat in urmatorul pas de timp)
			case 1:
				//printf ("Timp %d, Nod %d, msg tip 1 - LSA\n", timp, nod_id);
				break;

			case 2:
				//printf ("Timp %d, Nod %d, msg tip 2 - Database Request\n", timp, nod_id);
				break;

			case 3:
				//printf ("Timp %d, Nod %d, msg tip 3 - Database Reply\n", timp, nod_id);
				break;

			case 4:
				//printf ("Timp %d, Nod %d, msg tip 4 - pachet de date (de rutat)\n", timp, nod_id);
				break;

			case 6://complet in ceea ce priveste partea cu mesajele de control
					//puteti inlocui conditia de coada goala, ca sa corespunda cu implementarea personala
					//aveti de implementat procesarea mesajelor ce tin de protocolul de rutare
				{
				timp++;
				//printf ("Timp %d, Nod %d, msg tip 6 - incepe procesarea mesajelor puse din coada la timpul anterior (%d)\n", timp, nod_id, timp-1);

				//veti modifica ce e mai jos -> in scheletul de cod nu exista nicio coada
				int coada_old_goala = TRUE;

				//daca NU mai am de procesat mesaje venite la timpul anterior
				//(dar mai pot fi mesaje venite in acest moment de timp, pe care le procesez la t+1)
				//trimit mesaj terminare procesare pentru acest pas (tip 5)
				//altfel, procesez mesajele venite la timpul anterior si apoi trimit mesaj de tip 5
				while (!coada_old_goala) {
					//	procesez tote mesajele din coada old
					//	(sau toate mesajele primite inainte de inceperea timpului curent - marcata de mesaj de tip 6)
					//	la acest pas/timp NU se vor procesa mesaje venite DUPA inceperea timpului curent
//cand trimiteti mesaje de tip 4 nu uitati sa setati (inclusiv) campurile, necesare pt logare:  mesaj.timp, mesaj.creator, mesaj.nr_secv, mesaj.sender, mesaj.next_hop
					//la tip 4 - creator este sursa initiala a pachetului rutat
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
