#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

int main() {
    char arr[256]; strcpy(arr, "admin");
    strcpy(arr, encryptString(arr));
    printf("%s", arr);
    return 0;
}