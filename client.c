/* cliTCPIt.c - Exemplu de client TCP
   Trimite un numar la server; primeste de la server numarul incrementat.
         
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009

   Cod adaptat de catre Ursulean Ciprian in vederea realizarii proiectului My File Transfer Protocol
*/
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>


#define MAX_LINE 4096

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

/* vector pentru mesaje de cerere / request */
char dummy_message[256];

int fileExistsOnServer(int sd, const char* fileName) {
    // trimit numele fisierului catre server sa faca verificarea
    if (write(sd, fileName, sizeof(fileName)) < 0) {
        perror("[client]Eroare la write().\n");
        return errno;
    }

    // primesc statusul existentei fisierului
    int fileExists;
    if (read(sd, &fileExists, sizeof(fileExists)) < 0) {
        perror("[client]Eroare la read().\n");
        return errno;
    }

    return fileExists;
}

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

long long int getFileSize(const char *file_name) {
    struct stat st;   
    if (stat(file_name,&st)==0) {
        return (st.st_size);
    }
    else {
        return -1;
    }
}

int fileExists(const char* fileName) {
    return access( fileName, F_OK ) != -1;
}

void sendfile(FILE *fp, int sockfd, unsigned long long fileSize) {
    printf("Incepem transferul... \n");

    unsigned long long total_sent = 0;
    int n; 
    char sendline[MAX_LINE] = {0}; 
    while (fileSize > 0) {
        n = fread(sendline, sizeof(char), MAX_LINE, fp);
        fileSize -= n; // actualizez cat mai am ce citit
        total_sent += n;
        if (n != MAX_LINE && ferror(fp)) {
            perror("Read File Error");
            exit(1);
        }
        
        if (send(sockfd, sendline, n, 0) == -1) {
            perror("Can't send file");
            return;
        }
        memset(sendline, 0, MAX_LINE);
        printf("\33[2K\rTotal transferat: %llu bytes ", total_sent);
    }
    printf("\nFisier trimis cu succes!\n");
}

void writefile(FILE *fp, int sockfd, unsigned long long fileSize) {
    printf("Incepem transferul...\n");
    unsigned long long total_written = 0;
    ssize_t n;
    char buffer[MAX_LINE] = {0};
    while (fileSize > 0) {
        n = recv(sockfd, buffer, MAX_LINE, 0);
        fileSize -= n; // actualizez cat mai am de scris
        total_written += n;
        if (n == 0) {
            break;
        }
        if (n == -1) {
            perror("Eroare la recv!");
            exit(1);
        }
        if (fwrite(buffer, sizeof(char), n, fp) != n) {
            perror("Eroare la scriere!");
            return;
        }
        printf("\33[2K\rTotal transferat: %llu bytes ", total_written);
        fflush(stdout);
        memset(buffer, 0, MAX_LINE); 
    }
    printf("\nFisier primit cu succes!\n");
    fflush(stdout);
}

char* getOnlyFileName(char* path) {
    /*
    *   Functia returneaza doar numele simplu al fisierului
    */
    char* fileName = malloc(128 * sizeof(char));
    int n = strlen(path) - 1;
    int idx = 0;

    while (n >= 0 && path[n] != '/') {
        fileName[idx++] = path[n--];
    }
    fileName[idx] = '\0';
    for (int i = 0; i < idx / 2; ++i) {
        char tmp = fileName[i];
        fileName[i] = fileName[idx - i - 1];
        fileName[idx - i - 1] = tmp;
    }
    return fileName;
}

int printFileAttributes(int sd) {
    // primesc statusul operatiei stat
    int statusStat;
    if (read(sd, &statusStat, sizeof(int)) < 0) {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
    }

    if (statusStat == 0) {
        // primesc drepturile de acces solicitate
        char drepturiAcces[4][64];
        if (read(sd, drepturiAcces, sizeof(drepturiAcces)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }

        for (int i = 0; i < 4; ++i) {
            printf(ANSI_COLOR_YELLOW "%s\n" ANSI_COLOR_RESET, drepturiAcces[i]);
        }

        // primesc crearea fisierului de la server
        char creationTime[256];
        if (read(sd, creationTime, sizeof(creationTime)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }

        printf(ANSI_COLOR_YELLOW "%s\n" ANSI_COLOR_RESET, creationTime);

        // primesc modificarea fisierului de la server
        char modificationTime[256];
        if (read(sd, modificationTime, sizeof(modificationTime)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }
        printf(ANSI_COLOR_YELLOW "%s\n" ANSI_COLOR_RESET, modificationTime);

        // primesc celelalte atribute
        char alteProprietati[6][128];
        if (read(sd, alteProprietati, sizeof(alteProprietati)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }
        for (int i = 0; i < 6; ++i) {
            printf(ANSI_COLOR_YELLOW "%s\n" ANSI_COLOR_RESET, alteProprietati[i]);
        }
        return 1;
    }
    else {
        printf(ANSI_COLOR_RED "Eroare la afisarea proprietatilor. Asigurativa ca fisierul dorit exista!\n" ANSI_COLOR_RESET);
    }
}

int checkPathAcces(const char* path, const char* username) {
    /*
    *  incazul in care am folderul userdata urmatorul folder trebuie sa fie username,
    *  cu alte cuvinte nu putem accesa fisierele altor useri
    */
    char tmp[256];
    strcpy(tmp, "userdata/");
    strcat(tmp, username);
    if (strstr(path, "userdata/")) {
        if (strstr(strstr(path, "userdata/"), tmp) != NULL) return 1;
        else return 0;
    }
    return 1;
}

int main (int argc, char *argv[]) {
    int sd;			            // descriptorul de socket
    struct sockaddr_in server;	// structura folosita pentru conectare 

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
    server.sin_port = htons(port);
  
    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1) {
        perror("[client]Eroare la connect().\n");
        return errno;
    }
    
    char username_send[128];
    char password_send[128];
    char message[64];
  
    /* citirea raspunsului dat de server 
    (apel blocant pana cand serverul raspunde) */

    while (1) {
        // primim cererea de username de la server
        if (read(sd, message, sizeof(message)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }

        printf("%s", message);
        scanf("%s", username_send);
        // trimitem username-ul serverului
        if (write(sd, username_send,sizeof(username_send)) <= 0) {
            perror("[client]Eroare la write() spre server.\n");
            return errno;
        }

        // primim cererea de parola de la server
        if (read(sd, message, sizeof(message)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }

        printf("%s", message);
        scanf("%s", password_send);

        // criptam parola
        // strcpy(password_send, encryptString(password_send));

        // trimitem parola serverului
        if (write(sd, password_send, sizeof(password_send)) <= 0) {
            perror("[client]Eroare la write() spre server.\n");
            return errno;
        }

        // primim mesajul de status logare
        int logare_status;
        if (read(sd, &logare_status, sizeof(int)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }

        if (logare_status == 1) {
            printf(ANSI_COLOR_GREEN "WELLCOME BACK, %s!\n" ANSI_COLOR_RESET, username_send);
            break;
        }
        else {
            printf(ANSI_COLOR_RED "WRONG USERNAME OR PASSWORD, TRY AGAIN!\n" ANSI_COLOR_RESET);
        }
    }

    // primim meniul cu optiuni de la server
    char mainMenu[6][128];
    if (read(sd, mainMenu, sizeof(mainMenu)) < 0) {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
    }

    for (int i = 0; i < 6; ++i) {
        printf(ANSI_COLOR_YELLOW "%s\n" ANSI_COLOR_RESET, mainMenu[i]);
    }
    
    int optiune;
    char path[256];
    while (1) {
        printf("Introduceti optiunea: ");
        scanf("%d", &optiune);

        // trimit optiunea serverului
        if (write(sd, &optiune, sizeof(int)) < 0) {
            perror ("[client]Eroare la read() de la server.\n");
            return errno;
        }

        if (optiune == 1) {
            printf("Introduceti calea fisierului: ");
            scanf("%s", path);
            int flag = fileExists(path) && checkPathAcces(path, username_send);
            while (!flag) {
                printf(ANSI_COLOR_RED "Fisierul nu exista sau nu aveti acces la el\n" ANSI_COLOR_RESET);

                printf("Introduceti calea fisierului sau 0 pentru iesire: ");
                fflush(stdout);
                scanf("%s", path);
                if (strcmp(path, "0") == 0) {
                    break;
                }

                flag = fileExists(path) && checkPathAcces(path, username_send);
            }
            
            // trimit flagul serverului
            if (write(sd, &flag, sizeof(int)) < 0) {
                perror ("[client]Eroare la read() de la server.\n");
                return errno;
            }
            
            if (flag == 1) {
                // trimit serverului si calea unde va fi trimis fisierul
                char pathToServer[256];
                strcpy(pathToServer, "userdata/");
                strcat(pathToServer, username_send);
                strcat(pathToServer, "/incarcari/");
                strcat(pathToServer, getOnlyFileName(path));

                // trimit serverului calea fisierului primit
                if (write(sd, pathToServer, sizeof(pathToServer)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }

                FILE* filePtr = fopen(path, "rb");
                if (filePtr == NULL) {
                    printf(ANSI_COLOR_RED "Eroare la deschirea fisierului!\n" ANSI_COLOR_RESET);
                    fflush(stdout);
                    break;
                }

                unsigned long long fileSize = getFileSize(path);
                if (fileSize == -1) {
                    printf(ANSI_COLOR_RED "Eroare la calcularea dimensiunii fisierului!" ANSI_COLOR_RESET);
                    fflush(stdout);
                }

                // trimit serverului dimensiunea fisierului transferat
                if (write(sd, &fileSize, sizeof(int)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }

                // trimit fisierul serverului
                sendfile(filePtr, sd, fileSize);
                fclose(filePtr);
            }
        }
        else if (optiune == 2) {
            // trimit serverului si calea unde va fi trimis fisierul
            char pathToServer[256];
            strcpy(pathToServer, "userdata/");
            strcat(pathToServer, username_send);
            strcat(pathToServer, "/incarcari/");

            if (write(sd, pathToServer, sizeof(pathToServer)) < 0) {
                perror ("[client]Eroare la read() de la server.\n");
                return errno;
            }

            // primesc numarul de fisiere disponibile pentru descarcare
            int noOfFiles;
            if (read(sd, &noOfFiles, sizeof(int)) < 0) {
                perror ("[client]Eroare la read() de la server.\n");
                return errno;
            }

            if (noOfFiles == 0) {
                printf(ANSI_COLOR_RED "Nu aveti fisiere de descarcat!\n" ANSI_COLOR_RESET);
                fflush(stdout);
            } else {
                char fileName[256];
                for (int i = 0; i < noOfFiles; ++i) {
                    // primesc pe rand continutul disponibil de descarcat
                    if (read(sd, fileName, sizeof(fileName)) < 0) {
                        perror ("[client]Eroare la read() de la server.\n");
                        return errno;
                    }
                    printf(ANSI_COLOR_YELLOW "    %d. - %s\n" ANSI_COLOR_RESET, i + 1, fileName);
                    fflush(stdout);
                }

                // primesc cererea de a alege
                if (read(sd, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }

                printf("%s\n", dummy_message);

                int idx;
                scanf("%d", &idx);
                while (idx < 1 || idx > noOfFiles) {
                    printf(ANSI_COLOR_RED "Optiunea este de la 1 la %d, incercati din nou... " ANSI_COLOR_RESET, noOfFiles);
                    scanf("%d", &idx);
                }

                // trimit optiunea serverului
                if (write(sd, &idx, sizeof(int)) < 0) {
                    perror ("[client]Eroare la write() de la server.\n");
                    return errno;
                }

                // primesc numele fisierului descarcat
                if (read(sd, fileName, sizeof(fileName)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }

                // generez calea noului fisier ce urmeaza a fi descarcat
                char pathToDownload[512];
                strcpy(pathToDownload, "userdata/");
                strcat(pathToDownload, username_send);
                strcat(pathToDownload, "/descarcari/");
                strcat(pathToDownload, getOnlyFileName(fileName));

                // primesc dimensiunea fisierului
                int fileSize;
                if (read(sd, &fileSize, sizeof(int)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }

                // transmit fisierul
                FILE* filePtr = fopen(pathToDownload, "wb");
                if (filePtr == NULL) {
                    printf(ANSI_COLOR_RED "Eroare la deschiderea fisierului!\n" ANSI_COLOR_RESET);
                    return -1;
                }

                writefile(filePtr, sd, fileSize);
            }
        }
        else if (optiune == 3) {
            // primesc optiunile de la server
            char operatii[5][128];
            if (read(sd, operatii, sizeof(operatii)) < 0) {
                perror ("[client]Eroare la read() de la server.\n");
                return errno;
            }
            for (int i = 0; i < 5; ++i) {
                printf(ANSI_COLOR_CYAN "%s\n" ANSI_COLOR_RESET, operatii[i]);
            }
            
            char folderOperationsPath[256];
            char optiuniMessageRequest[256];
            char fileName[128];
            strcpy(folderOperationsPath, "userdata/");
            strcat(folderOperationsPath, username_send);
            strcat(folderOperationsPath, "/");

            // trimit serverului calea de baza unde vor fi facute operatiunile cu fisiere
            if (write(sd, folderOperationsPath, sizeof(folderOperationsPath)) < 0) {
                perror ("[client]Eroare la read() de la server.\n");
                return errno;
            }

            int optiune_operatii_fisiere;
            printf("    -Introduceti optiunea: ");
            scanf("%d", &optiune_operatii_fisiere);

            while (optiune_operatii_fisiere < 0 || optiune_operatii_fisiere > 6) {
                printf(ANSI_COLOR_RED "    -Optiunea este de la 1 la 4... " ANSI_COLOR_RESET);
                scanf("%d", &optiune_operatii_fisiere);
            }

            // trimit serverului optiunea aleasa
            if (write(sd, &optiune_operatii_fisiere, sizeof(int)) < 0) {
                perror ("[client]Eroare la write() de la server.\n");
                return errno;
            }

            if (optiune_operatii_fisiere == 1) {
                int fileOrDir;
                
                if (read(sd, optiuniMessageRequest, sizeof(optiuniMessageRequest)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }
                printf("%s", optiuniMessageRequest);

                scanf("%d", &fileOrDir);
                // trimit optiunea pentru fisier / director
                if (write(sd, &fileOrDir, sizeof(int)) < 0) {
                    perror ("[client]Eroare la write() de la server.\n");
                    return errno;
                }

                // primesc cererea de introducere a numelui fisierului / directorului
                if (read(sd, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }

                printf("%s    -", dummy_message);
                scanf("%s", fileName);

                // trimit numele fisierului / directorului de creat
                if (write(sd, fileName, sizeof(fileName)) < 0) {
                    perror ("[client]Eroare la write() de la server.\n");
                    return errno;
                }

                // primesc statusul comenzii de creare fisier / server
                if (read(sd, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }
                printf("%s    -\n", dummy_message);
            }
            else if (optiune_operatii_fisiere == 2) {
                // primesc cererea de introducere a numelui fisierului
                if (read(sd, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[client]Eroare la write() de la server.\n");
                    return errno;
                }
                printf("%s\n", dummy_message);
                fflush(stdout);
                scanf("%s", fileName);

                // trimit numele directorului de afisat
                if (write(sd, fileName, sizeof(fileName)) < 0) {
                    perror ("[client]Eroare la write() de la server.\n");
                    return errno;
                }

                // primesc statusul de corectitudine al existentei directorului
                int exista_director;
                if (read(sd, &exista_director, sizeof(int)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return errno;
                }
                if (exista_director == 1) {
                    // primesc numarul de iteme din fisier                    
                    int *noOfItems = malloc(sizeof(int));
                    if (read(sd, noOfItems, sizeof(noOfItems)) < 0) {
                        perror ("[client]Eroare la read() de la server.\n");
                        return errno;
                    }
                    for (int i = 0; i < *(noOfItems); ++i) {
                        // citesc itemul curent
                        if (read(sd, dummy_message, sizeof(dummy_message)) < 0) {
                            perror ("[client]Eroare la read() de la server.\n");
                            return errno;
                        }
                        printf(ANSI_COLOR_CYAN "        * %s\n" ANSI_COLOR_RESET, dummy_message);
                        fflush(stdout);
                    }
                }
                else {
                    printf(ANSI_COLOR_RED "    -Directorul intordus nu exista!\n" ANSI_COLOR_RESET);
                    fflush(stdout);
                }
            }
            else if (optiune_operatii_fisiere == 3) {
                // primesc solicitarea numelui fisierului
                if (read(sd, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return -1;
                }

                printf("%s", dummy_message);
                scanf("%s", fileName);

                // trimit numele fisierului
                if (write(sd, fileName, sizeof(fileName)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return -1;
                }

                // trimit proprietatile fisierului soliictat
                int recvProps = printFileAttributes(sd);
            }
            else if (optiune_operatii_fisiere == 4) {
                // primesc cererea de stergere de la server
                if (read(sd, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[server]Eroare la read() de la server.\n");
                    return -1;
                }
                printf("%s\n", dummy_message);
                fflush(stdout);
                scanf("%s", fileName);
                while (checkPathAcces(fileName, username_send) != 1) {
                    printf(ANSI_COLOR_RED "Nu aveti dreptul sa stegeti fisierul respectiv!\n" ANSI_COLOR_RESET);
                    printf("Introduceti numele fisierului: ");
                    fflush(stdout);
                    scanf("%s", fileName);
                }

                // trimit numele fisierului catre server
                if (write(sd, fileName, sizeof(fileName)) < 0) {
                    perror ("[server]Eroare la write() de la server.\n");
                    return -1;
                }

                // primesc statusul stergerii fisierului
                int status_stergere;
                if (read(sd, &status_stergere, sizeof(int)) < 0) {
                    perror ("[server]Eroare la read() de la server.\n");
                    return -1;
                }

                if (status_stergere == 1) {
                    printf(ANSI_COLOR_GREEN "Stergerea s-a efectuat cu succes!\n" ANSI_COLOR_RESET);
                    fflush(stdout);
                }
                else {
                    printf(ANSI_COLOR_RED "Eroare la stergerea fisierului!\n" ANSI_COLOR_RESET);
                    fflush(stdout);
                }
            }
            else {
                printf(ANSI_COLOR_RED "Introduceti o optiune valida!\n" ANSI_COLOR_RESET);
                fflush(stdout);
            }
        }
        else if (optiune == 0) {
            printf("Deconectat!\nLa revedere, %s!", username_send);
            break;
        }
        else {
            printf(ANSI_COLOR_RED "Optiunile valide sunt: 1, 2, 3, 0\n" ANSI_COLOR_RESET);
        }
    }

    /* inchidem conexiunea, am terminat */
    close (sd);
    return 0;
}
