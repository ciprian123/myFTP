/* servTCPConcTh2.c - Exemplu de server TCP concurent care deserveste clientii
   prin crearea unui thread pentru fiecare client.
   Asteapta un numar de la clienti si intoarce clientilor numarul incrementat.
	Intoarce corect identificatorul din program al thread-ului.
  
   
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009

   Cod adaptat de catre Ursulean Ciprian in vederea realizarii proiectului My File Transfer Protocol
*/

#include <sys/types.h>  // librarie pentru traduri si alte asemenea
#include <sys/socket.h> // librarie pentru lucru cu socket
#include <netinet/in.h> // librarie pentru configuratii socketuri
#include <errno.h>      // librarie pentru manipularea erorilor
#include <unistd.h>     // librarie pentru anumite constante folosite
#include <stdio.h>      // librarie pentru functii I/O
#include <string.h>     // libraria pentru functii pe siruri de caractere
#include <stdlib.h>     // libraria standard pentru functii
#include <signal.h>     // libraria pentru semnale
#include <pthread.h>    // libraria pentru tread-uri
#include <sys/wait.h>   // libraria pentru primitiva wait
#include <sys/stat.h>   // libraria pentru functii pe fisiere

#include <time.h>       // libaria pentru functii de timp

#include <dirent.h>     // libraria pentru lucru cu directoare

#include <sqlite3.h>    // interfata de lucru cu baza de date
#include <strings.h>    // pentru bzero si alte functii

/* portul folosit */
#define PORT 2908

#define MAX_LINE 4096

/* culorile consolei */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* vector pentru mesaje de cerere / request */
char dummy_message[256];


typedef struct thData {
	int idThread;        // id-ul thread-ului tinut in evidenta de acest program
	int cl;              // descriptorul intors de accept
} thData;

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);
int logare_helper(const char* , const char *);
int logare(struct thData tdL);
void raspunsLogare();
void sendMenu(struct thData tdL);
void sendMenuOperatiiFisiereSiDirectoare(struct thData tdL);
void computeOperations(struct thData tdL);
int createUserStorage(const char* username);
void writefile(int sockfd, FILE *fp, unsigned long long fileSize);
void sendfile(FILE *fp, int sockfd, unsigned long long fileSize);
int fileExists(const char* fileName);
void listFilesRecursively(char* path, struct thData tdL, int *countItems, int toSent);
long long int getFileSize(const char *file_name);
void getNthFileName(const char* basePath, int* noOfFiles, int idx, char* nThFileName);
int deleteFile(const char* fileName);
int sendFileAttributes(struct thData tdL, const char* fileName);
char* itoa(int val, int base);
int checkPathAcces(const char* path, const char* username);


int main () {
    struct sockaddr_in server;	// structura folosita de server
    struct sockaddr_in from;	
    int nr;		               //mesajul primit de trimis la client 
    int sd;		               //descriptorul de socket 
    int pid;
    pthread_t th[100];          //Identificatorii thread-urilor care se vor crea
    int i = 0;

    /* crearea unui socket */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
        perror ("[server]Eroare la socket().\n");
        return errno;
    }

    /* utilizarea optiunii SO_REUSEADDR */
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  
    // /* pregatirea structurilor de date */
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));
  
    /* umplem structura folosita de server */
    /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	

    /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);

    /* utilizam un port utilizator */
    server.sin_port = htons (PORT);
  
    /* atasam socketul */
    if (bind(sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1) {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, 2) == -1) {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    /* servim in mod concurent clientii...folosind thread-uri */
    while (1) {
        int client;
        thData * td; //parametru functia executata de thread     
        int length = sizeof (from);

        printf ("[server]Asteptam la portul %d...\n",PORT);
        fflush (stdout);

        // client= malloc(sizeof(int));
        /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
        if ((client = accept (sd, (struct sockaddr *) &from, &length)) < 0) {
            perror ("[server]Eroare la accept().\n");
            continue;
        }
        
        /* s-a realizat conexiunea, se astepta mesajul */
        
        // int idThread; //id-ul threadului
        // int cl; //descriptorul intors de accept

        td = (struct thData*)malloc(sizeof(struct thData));	
        td->idThread = i++;
        td->cl = client;

        pthread_create(&th[i], NULL, &treat, td);	              
    } //while    
}

static void *treat(void * arg) {		
	struct thData tdL; 
	tdL = *((struct thData*)arg);	
	fflush(stdout);		 
	pthread_detach(pthread_self());		
	raspunde((struct thData*)arg);
	
    /* am terminat cu acest client, inchidem conexiunea */
	close ((intptr_t)arg);
	return(NULL);		
};

int logare_helper(const char* username, const char* password) {
    // ne conectam la baza de date pentru autentificare
    char *err_msg = 0;
    struct sqlite3 *db;
    int rc = sqlite3_open("ftp_users.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    // Search the record in database in order to authenticate the user
    char SQL[256];
    strcpy(SQL, "SELECT * FROM USERS WHERE "); 
    strcat(SQL, "'"); 
    strcat(SQL, username);
    strcat(SQL, "'"); 
    strcat(SQL, " = USERS.username AND "); 
    strcat(SQL, "'");
    strcat(SQL, password); 
    strcat(SQL, "'"); 
    strcat(SQL, " = USERS.password AND BAN_STATUS = 0;");

    struct sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, SQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);        
        sqlite3_close(db);
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}

int logare(struct thData tdL) {
    char username_request[64];
    char password_request[64];
    char username_received[128];
    char password_received[128];

    while (1) {
        strcpy(username_request, "[server] USERNAME: ");
        strcpy(password_request, "[server] PASSWORD: ");
                
        // cer username-ul clientului
        if (write (tdL.cl, username_request, sizeof(username_request)) <= 0) {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }

        // primesc username-ul clientului
        if (read (tdL.cl, username_received, sizeof(username_received)) <= 0) {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }	

        // cer parola clinetului 
        if (write (tdL.cl, password_request, sizeof(password_request)) <= 0) {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }

        // primesc parola clientului
        if (read (tdL.cl, password_received, sizeof(password_received)) <= 0) {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }

        int response = logare_helper(username_received, password_received);
        // trimit mesajul de logare
        if (write (tdL.cl, &response, sizeof(int)) <= 0) {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }
        if (response == 1) {
            /*
            * =================================================================================================
            * fiecarui utilizator ii este asociat un folder, asociat in mod unic acestuia, iar pe acest
            * folder se vor executa anumite operatiuni, mai exact, cand se va face upload unui fisier,
            * aflat pe masina unde ruleaza clientul, fisierul va fi transferat servarului in fisierul asociat
            * clientului, in ierarhia de fisiere creata de el.
            * Astfel, fiecarui client ii este rezervat spatiul sau "pe server"
            * =================================================================================================
            */
            if (createUserStorage(username_received) < 0) {
                printf("Error at creating user storage!");
                return -1;
            }
            return 1;
        }
    }
    return 0;
}

void computeOperations(struct thData tdL) {
    // trimitem meniul cu optiuni clientului
    FILE *filePtr;
    int bytesReceived = 0;

    sendMenu(tdL);
    while (1) {
        //  primesc optiunea clientului
        int optiune;
        if (read (tdL.cl, &optiune, sizeof(int)) <= 0) {
            printf("[Thread %d] ",tdL.idThread);
            perror ("[Thread]Eroare la write() catre client.\n");
        }	
        printf("Optiunea este: %d\n", optiune);
        if (optiune == 1) { 
            // fisierele primite de la client vor fi incarcate pe server in storage-ul corespunzator
            // clientului, intr-un folder numit incarcari
            char pathToServer[256];

            // primesc flag de succes la path
            int flag;
            if (read(tdL.cl, &flag, sizeof(int)) < 0) {
                perror ("[client]Eroare la read() de la server.\n");
                return;
            }

            if (flag != 1) {
                // primesc calea fisierului trimis de client
                if (read(tdL.cl, pathToServer, sizeof(pathToServer)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return;
                }
                
                // primesc dimensiunea fisierului
                int fileSize;
                if (read(tdL.cl, &fileSize, sizeof(int)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return;
                }

                FILE* filePtr = fopen(pathToServer, "wb");
                if (filePtr == NULL) {
                    printf("Error at opening file!");
                }
                writefile(tdL.cl, filePtr, fileSize);
                fclose(filePtr);
            }
        }
        else if (optiune == 2) {
            char basePath[256];

            

            // primesc calea fisierului trimis de client
            if (read(tdL.cl, basePath, sizeof(basePath)) < 0) {
                perror ("[client]Eroare la read() de la server.\n");
                return;
            }

            // trimit clientului lista fisierelor incarcare
            int* noOfFiles = malloc(sizeof(int));
            (*noOfFiles) = 0;
            listFilesRecursively(basePath, tdL, noOfFiles, 0);
            
            int tmp = (*noOfFiles);
            if (write(tdL.cl, &tmp, sizeof(int)) < 0) {
                perror ("[client]Eroare la write() de la server.\n");
                return;
            }
            
            if (tmp != 0) {
                (*noOfFiles) = 0;
                listFilesRecursively(basePath, tdL, noOfFiles, 1);

                // trimit cererea de alegere clientului
                strcpy(dummy_message, "Introduceti numarul corespunzator fisierului dorit: ");
                if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[client]Eroare la write() de la server.\n");
                    return;
                }

                // primesc optiunea cientului
                int idx;
                if (read(tdL.cl, &idx, sizeof(int)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return;
                }

                char nThFileName[256];
                (*noOfFiles) = 0;
                getNthFileName(basePath, noOfFiles, idx, nThFileName);

                // trimit numele fisierului solicitat
                if (write(tdL.cl, nThFileName, sizeof(nThFileName)) < 0) {
                    perror ("[client]Eroare la read() de la server.\n");
                    return;
                }            

                int fileSize = getFileSize(nThFileName);
                if (fileSize < 0) {
                    printf("Eroare la calculul dimensiunii fisierului!");
                    fflush(stdout);
                    return;
                }

                // trimit clientului dimensiunea fisierului
                if (write(tdL.cl, &fileSize, sizeof(int)) < 0) {
                    perror ("[client]Eroare la write() de la server.\n");
                    return;
                }

                printf("nThFIleName: %s\n", nThFileName);

                // deschid fisierul solicitat pentru citire
                FILE* filePtr = fopen(nThFileName, "rb");
                if (filePtr == NULL) {
                    printf("Eroare la deschiderea fisierului!");
                    return;
                }

                // transfer fisierul
                sendfile(filePtr, tdL.cl, fileSize);
            }
        }
        else if (optiune == 3) {
            sendMenuOperatiiFisiereSiDirectoare(tdL);

            char folderOperationsPath[256];
            char optiuniMessageRequest[256];
            char fileName[128];
            char username[128];
            int optiuneOperatiiFisiere;

            // primesc calea de baza unde se vor executa operatiile cu fisiere
            if (read(tdL.cl, folderOperationsPath, sizeof(folderOperationsPath)) < 0) {
                perror ("[server]Eroare la read() de la server.\n");
                return;
            }

            // primesc optiunea aleaza de client
            if (read(tdL.cl, &optiuneOperatiiFisiere, sizeof(int)) < 0) {
                perror ("[server]Eroare la read() de la server.\n");
                return;
            }

            if (optiuneOperatiiFisiere == 1) {
                strcpy(optiuniMessageRequest, "    -Pentr crearea unui director introduceti 1\n");
                strcat(optiuniMessageRequest, "    -Pentru crearea unui fisier introduceti altceva\n");
                
                // trimit cerintele de creare fisier / director clientului
                if (write(tdL.cl, optiuniMessageRequest, sizeof(optiuniMessageRequest)) < 0) {
                    perror ("[server]Eroare la write() de la client.\n");
                    return;
                }

                int fileOrDir;
                // primesc optiunes de creare fisier / server
                if (read(tdL.cl, &fileOrDir, sizeof(int)) < 0) {
                    perror("[server]Eroare la read() de la client.");
                    return;
                }

                strcpy(dummy_message, "    -Introduceti numele fisierului / directoului... ");

                // trimit cererea pentru numele fisierului / directorului
                if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[server]Eroare la write() de la client.\n");
                    return;
                }

                // primesc numele fisierului / directorului
                if (read(tdL.cl, fileName, sizeof(fileName)) < 0) {
                    perror("[server]Eroare la read() de la client.");
                    return;
                }

                if (fileOrDir == 1) {
                    // creez director
                    if (mkdir(strcat(strcat(folderOperationsPath, fileName), "/"), 0777) != 0) {
                        printf("     -Eroare la creare director!\n");
                        fflush(stdout);

                        // trimit eroarea catre client
                        strcpy(dummy_message, "Eroare la creare director!");
                        if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                            perror ("[server]Eroare la write() de la client.\n");
                            return;
                        }
                    }
                    strcpy(dummy_message, "Director creat cu succes!");
                    // trimit statusul operatiei catre server
                    if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                        perror ("[server]Eroare la write() de la client.\n");
                        return;
                    }
                }
                else {
                    // creez fisier
                    FILE* filePtr = fopen(strcat(folderOperationsPath, fileName), "w");
                    if (filePtr == NULL) {
                        printf("    -Eroare la creare fisier!\n");
                        fflush(stdout);

                        // trimit eroarea catre client
                        strcpy(dummy_message, "Eroare la creare fisier!");
                        if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                            perror ("[server]Eroare la write() de la client.\n");
                            return;
                        }
                    }
                    else {
                        // trimit statusul operatiei catre server
                        strcpy(dummy_message, "    -Fisier creat cu succes!");
                        if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                            perror ("[server]Eroare la write() de la client.\n");
                            return;
                        }
                    }
                }
            }
            else if (optiuneOperatiiFisiere == 2) {
                // trimit cererea de a introduce numele unui director
                strcpy(dummy_message, "    -Introduceti numele directorului... ");
                if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[server]Eroare la read() de la server.\n");
                    return;
                }

                // primesc numele directorului de parcurs
                if (read(tdL.cl, fileName, sizeof(fileName)) < 0) {
                    perror ("[server]Eroare la read() de la server.\n");
                    return;
                }

                int exists_status = fileExists(strcat(folderOperationsPath, fileName));
                // trimit statusul existentei directorului catre server
                if (write(tdL.cl, &exists_status, sizeof(int)) < 0) {
                    perror ("[server]Eroare la write() de la server.\n");
                    return;
                }

                if (exists_status != 0) {
                    int *noOfItems = malloc(sizeof(int));
                    *(noOfItems) = 0;
                    // numar cate iteme sunt in director si trimit clientului
                    listFilesRecursively(folderOperationsPath, tdL, noOfItems, 0);
                    if (write(tdL.cl, noOfItems, sizeof(noOfItems)) < 0) {
                        perror ("[server]Eroare la read() de la server.\n");
                        return;
                    }
                    printf("no of items: %d\n", *(noOfItems));
                    fflush(stdout);
                    // trimit itemele clientului
                    *(noOfItems) = 0;
                    listFilesRecursively(folderOperationsPath, tdL, noOfItems, 1);
                }
            }
            else if (optiuneOperatiiFisiere == 3) {
                // afisati proprietatile unui fisier pe server
                
                // trimit soliictarea numelui fisierului
                strcpy(dummy_message, "Introduceti numele fisierului: ");
                if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[server]Eroare la write() de la server.\n");
                    return;
                }

                // primesc numele fisierului
                if (read(tdL.cl, fileName, sizeof(fileName)) < 0) {
                    perror ("[server]Eroare la read() de la server.\n");
                    return;
                }

                // trimit proprietatile fisierului soliictat
                int sendProps = sendFileAttributes(tdL, fileName);
            }
            else if (optiuneOperatiiFisiere == 4) {
                // sterg fisier de pe server

                // trimit cererea de stergere la client
                strcpy(dummy_message, "Introdceti fisierul dorit: ");
                if (write(tdL.cl, dummy_message, sizeof(dummy_message)) < 0) {
                    perror ("[server]Eroare la write() de la server.\n");
                    return;
                }

                // primesc numele fisierului de sters
                if (read(tdL.cl, fileName, sizeof(fileName)) < 0) {
                    perror ("[server]Eroare la read() de la server.\n");
                    return;
                }

                // trimit statusul stergerii catre client
                int deleteStatus = deleteFile(fileName);
                if (write(tdL.cl, &deleteStatus, sizeof(int)) < 0) {
                    perror ("[server]Eroare la write() de la server.\n");
                    return;
                }
            }
        }
        else {
            break;
        }
    }
}

char* getExtension(const char* path) {
    /*
    *   Functia returneaza extensia fisierului
    */
    char* fileExt = malloc(128 * sizeof(char));
    int n = strlen(path) - 1;
    int idx = 0;

    while (n >= 0 && path[n] != '.') {
        fileExt[idx++] = path[n--];
    }
    fileExt[idx] = '\0';
    for (int i = 0; i < idx / 2; ++i) {
        char tmp = fileExt[i];
        fileExt[i] = fileExt[idx - i - 1];
        fileExt[idx - i - 1] = tmp;
    }
    return fileExt;
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

void sendMenu(struct thData tdL) {
    char mainMenu[6][128];
    strcpy(mainMenu[0], "***************************************************");
    strcpy(mainMenu[1], "1. Transfer de fisiere de la client la server");
    strcpy(mainMenu[2], "2. Transfer de fisiere de la server la client");
    strcpy(mainMenu[3], "3. Operatii cu fisiere si directoare");
    strcpy(mainMenu[4], "0. Deconectare");
    strcpy(mainMenu[5], "***************************************************");

    // trimit meniul cu optiuni clientului
    if (write (tdL.cl, mainMenu, sizeof(mainMenu)) <= 0) {
        printf("[Thread %d] ",tdL.idThread);
        perror ("[Thread]Eroare la write() catre client.\n");
    }
}

void sendMenuOperatiiFisiereSiDirectoare(struct thData tdL) {
    char optiuni[5][128];
    strcpy(optiuni[0], "    1. Creati un fisier");
    strcpy(optiuni[1], "    2. Afisati contiutul unui director");
    strcpy(optiuni[2], "    3. Afisati proprietatile unui fisier sau a unui director");
    strcpy(optiuni[3], "    4. Stergeti un fisier sau un director");
    strcpy(optiuni[4], "    0. Nicio optiune.");

    // trimit optiunile clientului
    if (write (tdL.cl, optiuni, sizeof(optiuni)) <= 0) {
        printf("[Thread %d] ",tdL.idThread);
        perror ("[Thread]Eroare la write() catre client.\n");
    }
}

void raspunde(void *arg) {
	struct thData tdL;
	tdL = *((struct thData*)arg);
    int status_logare = logare(tdL);
    if (status_logare == 1) {
        computeOperations(tdL);
    }
}

int createUserStorage(const char* username) {
    char storagePath[512];
    strcpy(storagePath, "/home/ciprianos/Desktop/myFTP/userdata/");
    strcat(storagePath, username);
    strcat(storagePath, "/");

    DIR* dir = opendir(storagePath);
    if (dir) {
        /* Directory exists. */
        closedir(dir);
        return 1;
    }
    else if (ENOENT == errno) {
        if (mkdir(storagePath, 0777) != 0) {
            printf("Eroare la creare: %s\n", storagePath);
            return -1;
        }

        char tmp[512];
        strcpy(tmp, storagePath);
        strcat(tmp, "descarcari/");
        if (mkdir(tmp, 0777) != 0) {
            printf("Eroare la creare: %s\n", tmp);
            return -1;
        }

        strcat(storagePath, "incarcari/");
        if (mkdir(storagePath, 0777) != 0) {
            printf("Eroare la creare: %s\n", storagePath);
            return -1;
        }
        return 0;
    }
    return -1;
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

void writefile(int sockfd, FILE *fp, unsigned long long fileSize) {
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

void listFilesRecursively(char *basePath, struct thData tdL, int *countItems, int toSent) {
    char path[1024];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    // nu putem deschide fisierul
    if (!dir) {
        return;
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            (*countItems)++; // actualizez numarul de iteme
            if (toSent == 1) {
                // trimit clientului fisierul curent
                if (write(tdL.cl, dp->d_name, sizeof(dp->d_name)) < 0) {
                    perror ("[server]Eroare la read() de la server.\n");
                    return;
                }
            }

            // concatenam fisiere la calea deja existenta, pentru a avansa un subfoldere
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);

            listFilesRecursively(path, tdL, countItems, toSent);
        }
    }
    closedir(dir);
}

void getNthFileName(const char* basePath, int* noOfFiles, int idx, char* nThFileName) {
    char path[1024];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    // nu putem deschide fisierul
    if (!dir) {
        return;
    }

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            (*noOfFiles)++; // actualizez numarul de iteme

            // concatenam fisiere la calea deja existenta, pentru a avansa un subfoldere
            strcpy(path, basePath);
            // strcat(path, "/");
            strcat(path, dp->d_name);

            if ((*noOfFiles) == idx) {
                strcpy(nThFileName, path);
                return;
            }
            getNthFileName(path, noOfFiles, idx, nThFileName);
        }
    }
    closedir(dir);
}

int deleteFile(const char* fileName) {
    printf(ANSI_COLOR_RED "Atentie, stergerea unui fisier de pe server este ireversibila!\n" ANSI_COLOR_RESET);
    fflush(stdout);
    return (remove(fileName) == 0);
}

int sendFileAttributes(struct thData tdL, const char* fileName) {
    struct stat stats;
    struct tm dt;

    int statStatus = stat(fileName, &stats);
    // trimit statusul operatiei stat
    if (write(tdL.cl, &statStatus, sizeof(int)) < 0) {
        perror ("[server]Eroare la write() de la server.\n");
        return -1;
    }

    if (statStatus == 0) {
        char drepturiAcces[4][64];

        int dreptCitire    = stats.st_mode & R_OK;
        int dreptScriere   = stats.st_mode & W_OK;
        int dreptExecutare = stats.st_mode & X_OK;

        strcpy(drepturiAcces[0], "Drepurile de acces ale fisierului:");

        strcpy(drepturiAcces[1], "     *Drept de citire: ");
        strcat(drepturiAcces[1], dreptCitire ? "DA" : "NU");

        strcpy(drepturiAcces[2], "     *Drept de scriere: ");
        strcat(drepturiAcces[2], dreptScriere ? "DA" : "NU");

        strcpy(drepturiAcces[3], "     *Drept de executare: ");
        strcat(drepturiAcces[3], dreptExecutare ? "DA" : "NU");

        // trimit clientului drepturile de acces 
        if (write(tdL.cl, drepturiAcces, sizeof(drepturiAcces)) < 0) {
            perror ("[server]Eroare la write() de la server.\n");
            return -1;
        }

        dt = *(gmtime(&stats.st_ctime));

        char creationTime[256];
        strcpy(creationTime, "Creat la: ");

        strcat(creationTime, itoa(dt.tm_mday, 10));
        strcat(creationTime, "-");

        strcat(creationTime, itoa(dt.tm_mon, 10));
        strcat(creationTime, "-");

        strcat(creationTime, itoa(dt.tm_year + 1900, 10));
        strcat(creationTime, " ");

        strcat(creationTime, itoa(dt.tm_hour, 10));
        strcat(creationTime, ":");

        strcat(creationTime, itoa(dt.tm_min, 10));
        strcat(creationTime, ":");
        
        strcat(creationTime, itoa(dt.tm_sec, 10));

        // trimit data crearii fisierului
        if (write(tdL.cl, creationTime, sizeof(creationTime)) < 0) {
            perror ("[server]Eroare la write() de la server.\n");
            return -1;
        }

        // File modification time
        dt = *(gmtime(&stats.st_mtime));

        char lastModificationTime[256];
        strcpy(lastModificationTime, "Modificat la: ");

        strcat(lastModificationTime, itoa(dt.tm_mday, 10));
        strcat(lastModificationTime, "-");

        strcat(lastModificationTime, itoa(dt.tm_mon, 10));
        strcat(lastModificationTime, "-");

        strcat(lastModificationTime, itoa(dt.tm_year + 1900, 10));
        strcat(lastModificationTime, " ");

        strcat(lastModificationTime, itoa(dt.tm_hour, 10));
        strcat(lastModificationTime, ":");

        strcat(lastModificationTime, itoa(dt.tm_min, 10));
        strcat(lastModificationTime, ":");
        
        strcat(lastModificationTime, itoa(dt.tm_sec, 10));

        // trimit data modificarii fisierului
        if (write(tdL.cl, lastModificationTime, sizeof(creationTime)) < 0) {
            perror ("[server]Eroare la write() de la server.\n");
            return -1;
        }

        char alteProprietati[6][128];
        strcpy(alteProprietati[0], "ID-ul device-ului unde este fisierul: ");

        strcat(alteProprietati[0], itoa(stats.st_dev, 10));

        strcpy(alteProprietati[1], "Numar inode: ");
        strcat(alteProprietati[1], itoa(stats.st_ino, 10));

        strcpy(alteProprietati[2], "Protectie: ");
        strcat(alteProprietati[2], itoa(stats.st_mode, 10));

        strcpy(alteProprietati[3], "Numar de hard links: ");
        strcat(alteProprietati[3], itoa(stats.st_nlink, 10));

        strcpy(alteProprietati[4], "ID-ul utilizatorului detinator: ");
        strcat(alteProprietati[4], itoa(stats.st_uid, 10));

        strcpy(alteProprietati[5], "ID-ul grupului detinator: ");
        strcat(alteProprietati[5], itoa(stats.st_gid, 10));

        // trimit celelalte proprietati clientului
        if (write(tdL.cl, alteProprietati, sizeof(alteProprietati)) < 0) {
            perror ("[server]Eroare la write() de la server.\n");
            return -1;
        }
        return 1;
    }
    return -1;
}

// functie de implementare itoa, variata aproape standard preluata de pe stackverflow.com
char* itoa(int val, int base){
	static char buf[32] = {0};
	int i = 30;
	for(; val && i ; --i, val /= base)
		buf[i] = "0123456789abcdef"[val % base];
	return &buf[i+1];
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