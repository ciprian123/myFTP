/* cliTCPIt.c - Exemplu de client TCP
   Trimite un numar la server; primeste de la server numarul incrementat.
         
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009

   Cod adaptat de catre Ursulean Ciprian in vederea realizarii proiectului My File Transfer Protocol
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

char* encryptString(const char* data) {
    /* 
     * creeam un sir nou in care pe pozitia i va contine codul ascii a lui data[i] la care adaug lungimea
     * sirului data + 5
     */
    char* encryptedData = malloc(sizeof(data) * sizeof(char));
    size_t tmp = strlen(data);
    for (int i = 0; i < tmp; ++i) {
        encryptedData[i] = (int)(data[i]) + tmp + 5;
    }
    return encryptedData;
}

char* decryptString(const char* data) {
    /*
    *  creeam un sir nou care pe pozitia i va contine caracterul corespunzator lui data[i] minus lungimea lui 
    *  data minus 5
    */
    char* decryptedData = malloc(sizeof(data) * sizeof(char));
    size_t tmp = strlen(data);
    for (int i = 0; i < tmp; ++i) {
        decryptedData[i] = (char)(data[i] - tmp - 5);
    }
    return decryptedData;
}

int main (int argc, char *argv[]) {
    int sd;			            // descriptorul de socket
    struct sockaddr_in server;	// structura folosita pentru conectare 

    // mesajul trimis
    int nr = 0;
    char buf[10];

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3) {
        printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi (argv[2]);

    /* cream socketul */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
        perror ("Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;

    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(argv[1]);

    /* portul de conectare */
    server.sin_port = htons (port);
  
    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1) {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    /* citirea mesajului */
    printf("[client]Introduceti un numar: ");
    fflush(stdout);
    read (0, buf, sizeof(buf));
    nr = atoi(buf);
    //scanf("%d",&nr);
  
    printf("[client] Am citit %d\n",nr);

    /* trimiterea mesajului la server */
    if (write(sd,&nr,sizeof(int)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
    }

    /* citirea raspunsului dat de server 
     (apel blocant pana cand serverul raspunde) */
    if (read(sd, &nr,sizeof(int)) < 0) {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
    }

    /* afisam mesajul primit */
    printf ("[client]Mesajul primit este: %d\n", nr);

    /* inchidem conexiunea, am terminat */
    close (sd);

    return 0;
}
