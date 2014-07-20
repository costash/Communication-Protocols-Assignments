/* Constantin Serban-Radoi 323CA
 * Tema 1 Protocoale de Comunicatii
 * Martie 2012
 *
 * readme.txt
 */

*** Brief ***
    Aceasta tema a presupus implementarea unui protocol cu fereastra glisanta
pentru a transmite un fisier de la un sender, catre un reciever.
    Am ales sa implementez un protocol de tip Selective Repeat, deoarece acesta
asigura o viteza mai buna in cazul in care sunt multe pachete corupte.

*** Structura fisierelor ***
    ==send.c    contine implementarea transmitatorului
    ==recv.c    contine implementarea receptorului
    ==crc.c     contine implementarea functiilor pentru calculul sumei de control
    ==crc.h     contine definitiile functiilor pentru calculul sumei de control
    ==Makefile  fisier pentru compilare automata a surselor

    Mentionez ca fisierele crc.c respectiv crc.h sunt cele folosite la laborator

*** Detalii de implementare ***
    Pentru <<<sender>>>, am calculat dimensiunea ferestrei in functie de delay
si speed, urmand ca apoi sa trimit un pachet de "instiintare" catre reciever,
pentru a-i trimite numele fisierului, dimensiunea sa si dimensiunea ferestrei.
Mesajul este retrimis pana cand primesc acknowledge pentru el pentru a fi siguri
ca putem incepe transferul mai departe.

    Am folosit o coada, implementata ca vector, cu capacitatea maxima de
dimensiunea ferestrei. Cand trimit un pachet la reciever, il adaug in coada, apoi
astept raspuns de la reciever. (De fiecare data incerc sa am coada cat mai plina
inainte sa astept raspuns)

    Daca raspunsul primit este NAK (mesaj corupt, sau in afara ferestrei),
retrimit ultimul mesaj din coada si astept alt raspuns.
    Daca raspunsul primit este ACK (mesaj acceptat, in interiorul ferestrei),
descarc varful cozii, si retrimit toate mesajele pana dau de mesajul pentru
care am primit acknowledge, apoi astept un nou raspuns.
    Daca nu primesc raspuns dupa timeout, retrimit ultimul mesaj din coada si
astept alt raspuns.

    Pentru <<<reciever>>>, am asteptat mesajul de Handshake pana s-a transmis corect
si am trimis catre sender dimensiunea ferestrei pe care o are reciever-ul.

    Cat timp am ce scrie in fisier, astept mesaj de la sender.
        Daca mesajul este corupt, sau in afara ferestrei este aruncat, si se
      trimite NAK, asteptand alt mesaj
        Daca mesajul este corect, trimit ACK, il salvez in BUFFER, apoi incerc
      sa scriu in fisier toate pachetele care sunt in ordine, incepand cu cel
      de la care s-a oprit scrierea.

*** Alte detalii ***
    Pentru a asigura o buna functionare, am considerat un minim de 10 cadre intr-o
fereastra.
    Dimensiunile ferestrelor celor 2 entitati sunt egale, avand numere de secventa
de cel putin doua ori mai mari.
    Am folosit o structura proprie, pe care am cast-uit-o ulterior la structura
originala, msg.
    Prin folosirea cozii se asigura ca se transmit date in continuu, folosindu-se
la maximum viteza legaturii.

    Pentru ultima cerinta, am considerat drept limitare a vitezei senderului,
faptul ca ii limitez fereastra la minimul dintre dimensiunea calculata de el, si
cea primita de la reciever, dar nu mai mica de 10. Astfel receptorul mai lent
va avea timp sa tina pas cu senderul.

*** Mentiuni continut arhiva ***
    Am inclus si fisierul run_experiment.sh folosit, pentru a sugera tipul
si ordinea parametrilor de la sender.
