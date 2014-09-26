#include <perror.h>

/* http://www.virtsync.com/c-error-codes-include-errno */
char * errorCodes[34] = {
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Argument list too long",
    "Exec format error",
    "Bad file number",
    "No child processes",
    "Try again",
    "Out of memory",
    "Permission denied",
    "Bad address",
    "Block device required",
    "Device or resource busy",
    "File exists",
    "Cross-device link",
    "No such device",
    "Not a directory",
    "Is a directory",
    "Invalid argument",
    "File table overflow",
    "Too many open files",
    "Not a typewriter",
    "Text file busy",
    "File too large",
    "No space left on device",
    "Illegal seek",
    "Read-only file system",
    "Too many links",
    "Broken pipe",
    "Math argument out of domain of func",
    "Math result not representable",
};

void perror(char *str) {
    if(strlen(str)>0) {
        write(1, str, strlen(str));
        char *separador = ":";
        write(1, separador, strlen(separador));
    }
    char * errorMessage;
    errorMessage = errorCodes[errno];
    write(1, errorMessage, strlen(errorMessage));
}
