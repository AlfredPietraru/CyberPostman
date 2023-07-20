## CyberPostman

## Disclaimer:
Inainte de toate, portiuni cod sunt preluate din scheletul de 
laborator si de resursa aceasta, https://beej.us/guide/bgnet/html/#poll 
care facea parte tot din recomandarile de la laboratorul 7.
Creare celor 2 sockets de tip UDP si cel de tip TCP, 
si multiplexarea input-ului. Deci functia :
int generate_socket_udp(char *port, int sock_type);
primele 50 de linii din server.c 
si de la randurile 140 - 170 in subscriber.c. Desi cred ca aceasta
este procedura standard pentru a defini sockets si ai conecta la 
un alt proces, cred ca tot trebuie spus ca am luat lucrurile astea.
Functia de hash pe care o folosesc in hashmap este luata de pe 
StackOverflow, nu mai stiu exact link-ul, il voi cauta. 
https://stackoverflow.com/questions/7666509/hash-function-for-string 

Disclaimer, stiu ca fisierele de tipul header cum ar fi common.h ar fi
trebuit sa aiba doar antetele functiilor, si un alt fisier de exemplu
common.c sa aiba implementarea. As fi facut in mod normal asa, dar am 
preferat sa pot avea acces la debbuger-ul pe care il pune la dispozitie
visual studio code. Am incercat sa il configurez astfel incat sa imi creeze
executabilul cu mai multe fisiere C, dar chiar daca reuseam treaba asta,
nu imi putea trece de la un fisier la altul, in momentul in care dadeam
breakpoint-uri, iar gdb-ul in linia de comanda mi se pare greoi, si ineficient
de folosit. Niste sugestii in legatura cu problema asta, pentru a stii sa 
configurez proiectele viitoare ar ajuta super mult. Am sa atasez si folderul
de vscode, pentru a vedea modul in care am facut configurarea debugger-ului
pana la urma.

## Cum rulam programul?
Am lucrat pe o masina de Ubuntu 22.04, si folosesc un Makefile, asa ca 
pentru a compila rulam:
```
make
```

In urma rularii se obtin 2 executabile "server" si "subscriber".
Se alege un port, de preferat 12345 cum se afla in fisierul test.py
din folderul sursa si se executa cu urmatorii parametrii:
``` 
./server <port dorit>
./subscriber <id client> <ip server> <prot dorit>
```

"id client" - trebuie sa fie un string de maxim 10 caractere care sa diferentieze
subscriberii intre ei, se pot conecta mai multi subscriberi la acelasi server
simultan.
"ip server" - totul se executa local, deci adresa IP trebuie sa fie 127.0.0.1

Pentru a testa atat serverul cat si subscriberul se vor folosi 2 script-uri de 
Python:  pcom_hw2_dup_client/upd_client.py, acesta genereaza mesaje de tip UDP
si le transmite la server, serverul are datoria sa le trimita mai departe 
la subscriberii potriviti. Exista mai multe tipuri de comenzi :
```
python3 udp_client.py 127.0.0.1 <port dorit>
python3 udp_client.py --mode manual 127.0.0.1 <port dorit>
python3 udp_client.py --source-port 1234 --input_file three_topics_payloads.json --mode random --delay 2000 127.0.0.1 <port dorit>
```

Iar pentru o testare mai generala se poate folosi script-ul test.py
```
sudo python3 test.py
```


## Detali de implementare server:
Am folosit functia poll pentru multiplexarea input-ului si output-ului.
Serverul l-am facut pentru o capacitate de 32 de subscriberi, dar acest lucru 
se poate ajusta, deoarece componentele sunt alocate dinamic, in functie de 
capacitatea initiala pe care o setam din #define SERVER_CAPACITY 32 aflat la
linia 10 in server.c 
Am creat cei 2 sockets, unul de UDP, celalalt de TCP. Cel de TCP, asteapta 
sa se conecteze noi clienti la server. Initiliazam o instanta de struct server
care va contine informatii despre utilizatori si mesajele aflate in asteptare
pentru clientii deconectati, dar si un hashmap al tuturor topicurilor posibile. 
Mersul programului este urmatorul : se conecteaza un client la server cu un anumit 
id, se creaza o structura de tip struct subscriber, definita in hashmap.h.
struct subscriber_node este un wrapper peste struct subscriber, pentru a putea
introduce acelasi subscriber in mai multe liste diferite, iar in cazul unei,
modificari, acesta trebuind sa fie facuta o singura data pentru a avea efect 
asupra tuturor instantelor de tip subscriber_node care au acceasi referinta 
de tip subscriber. Daca exista un subscriber cu acelasi id, structura este 
dezalocata si se inchide socket-ul nou creat pentru a inchide si subscriber-ul.
Se verifica in lista de tip history a server-ului. Daca exista un subscriber
cu acelasi id, se actualizeaza socket-ul subscriber-ului care s-a reconectat
si este mutat din history in lista de subscriberi activi.

Protocol creat se afla in fisierul udp_msg.h, si este struct my_protocol,
primii 8 bytes ai acestui protocol sunt completati in momentul venirii mesajului:
struct my_protocol {
    uint8_t ip_vector[4]; --adresa IP a clientului UDP
    uint16_t port; --portul pe care a tranmis clientul UDP  
    uint16_t size; --dimensiunea totala a pachetului trimis 
    char topic[50];
    uint8_t type;
}__attribute__((packed));
     
size - este reprezentat de cei 8 bytes pe care ii completeaza serverul la
venirea pachetului, plus dimensiunea totala a pachetului.
Se cauta topicul in hashmap, iar aici pot vorbi de structura din hashmap.h: 
    struct topic_node {
    char *name;
    struct subscriber_node *list_SF_1;
    struct subscriber_node *list_SF_0;
    struct promise *promise_list;
    struct topic_node *next;
};  

Practic serverul este orientat pe topicuri si nu pe clienti, topicul
este cel care detine informatii despre ce user este abonat si unde.
Functia send_all_promisies(&s, old_sub) verifica daca old_sub dat ca 
parametru are, in vreunul dintre topicuri un mesaj aflat in asteptare, si
daca il gaseste il trimite pe loc.

    struct promise {
        int size;
        char *msg;
        struct subscriber *sub;
        struct promise *next;
    };

In aceasta structura se afla stocat un mesaj care asteapta sa fie trimis,
acestea sunt stocate sub forma unei liste, o abordare mai eficienta ar fi
fost sa creez un hashmap, care sa aiba ca si cheie id_ul user-ului si ca 
valoare lista de mesaje, dar ar fi insemnat implementarea de la 0 a unui 
alt hashmap, sau transformarea primului astfel incat sa aiba valori de tip
void *, si ar fi afectat o parte din functiile pe care le-am facut inevitabil.

## Detalii de implementare subscriber :
Se dechide socket-ul de tip TCP, si se asteapta primirea de mesaje sau 
trimiterea de comenzi.
Am creat urmatoarea structura pentru a da o forma fixa mesajelor ce poate 
fi usor de interpretat de catre server
        struct command_protocol {
        char command_type;
        char topic[51];
        char argument;
    }__attribute__((packed));
 
Functia  read_one_message(sockfd_tcp, message, MSG_SIZE), face recv pentru
informatiile venite de la server, initial citeste exact 
sizeof(struct my_protocol), pentru a avea informatiile necesare despre pachet,
ulterior, facand citirea restului de date, pe care le afiseaza dupa formatul
potrivit.

Functiile de error_handler(condition, bla bla bla) opresc serverul cu totul
sunt erori grave
warning_handler(condtion, bla bla bla) sunt lucruri peste care se pot trece
peste, dar tot gresite, de exemplu o comanda introdusa prost.

## Probleme intampinate:
O problema destul de mare pe care am avut-o cu checker-ul a fost faptul ca
intreruperea brusca a acestuia, din cauza mea sau a unei erori, parea sa imi
strice buffer-ele de sistem, deoarece la urmatoarea rulare a serverului si 
a subscriber-ului se trimiteau FOARTE lent pachetele, inteleg sa se blocheze
recv-ul, putea fi eroare de la modul in care am implementat eu trimitera
pachetelor, dar se bloca send-ul, iar cantitatea de informatie transportata era
mult prea mica pentru a avea sens. Din cauza asta, primeam mesajele corecte,
dar ajungea sa dureze mult timp si primeam TIMEOUT, care mi-a mancat zilele.

Plus, mi se pare destul de ciudat sa avem de implementat comanda de 
unsubscribe, dar nu avem nici macar un test care sa o acopere. Inteleg ca  
checker-ul este mai mult orientativ si nu are treaba atat de mult cu 
punctajul final, dar ar fi prins bine macar un test.  