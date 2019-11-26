#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

int main() {

    return 0;
}


CREATE TABLE USERS (
    ID INTEGER PRIMARY KEY AUTOINCREMENT, 
    USERNAME VARCHAR(64) NOT NULL,
    PASSWORD VARCHAR(64) NOT NULL,
    BAN_STATUS INT NOT NULL
);