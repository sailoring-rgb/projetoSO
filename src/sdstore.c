// Client
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define argCountError "[ERROR] Insufficient number of arguments.\n"
#define argError "[ERROR] Invalid argument.\n"

int main(int argc, char *argv[]){
    if(argc < 2){
        write(STDOUT_FILENO, argCountError, sizeof(argCountError));
        return 0;
    }
    else{
        if(strcmp(argv[1],"status") == 0){
            // code for status option
        }
        else if (strcmp(argv[1],"proc-file") == 0){
            // code for proc-file option

        }
        else
            write(STDOUT_FILENO, argError, sizeof(argError));
    }
    return 0;
}