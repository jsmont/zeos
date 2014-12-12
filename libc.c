/*
* libc.c
*/

#include <libc.h>
#include <types.h>
#include <errno.h>

const char * errorCodes[34] = {
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

int errno;

void perror(char *str) {
    if(strlen(str)>0) {
        write(1, str, strlen(str));
        char *separador = ":";
        write(1, separador, strlen(separador));
    }
    char * errorMessage;
    errorMessage = errorCodes[geterrno()];
    write(1, errorMessage, strlen(errorMessage));
}

int write(int fd, char *buffer, int size) {
    int out;
    __asm__ (
	    "pushl %%ebx;"
        "movl $4, %%eax;"
        "movl %1, %%ebx;"
        "movl %2, %%ecx;"
        "movl %3, %%edx;"
        "int $0x80;"
        "movl %%eax, %0;"
	    "popl %%ebx;"
        :"=r" (out)
        :"r" (fd)
        ,"r" (buffer)
        ,"r" (size)
        :"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

void exit() {
    __asm__ (
        "movl $1, %eax;"
        "int $0x80;"
        );
}

int fork() {
    int out;
    __asm__ (
        "movl $2, %%eax;"
        "int $0x80;"
        "movl %%eax, %0"
        :"=r" (out)
        ::"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

int getpid() {
    int out;
    __asm__ (
        "movl $20, %%eax;"
        "int $0x80;"
        "movl %%eax, %0"
        :"=r" (out)
        ::"%eax"
        );
    return out;
}

int get_stats(int pid, struct stats * s) {
    int out;
    __asm__ (
        "movl $35, %%eax;"
        "movl %1, %%ebx;"
        "movl %2, %%ecx;"
        "int $0x80;"
        "movl %%eax, %0;"
        :"=r" (out)
        :"r" (pid)
        ,"r" (s)
        :"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

void itoa(int a, char * b)
{
    int i, i1;
    char c;

    if (a==0) { b[0]='0'; b[1]=0; return ;}

    i=0;
    while (a>0)
    {
        b[i]=(a%10)+'0';
        a=a/10;
        i++;
    }

    for (i1=0; i1<i/2; i1++)
    {
        c=b[i1];
        b[i1]=b[i-i1-1];
        b[i-i1-1]=c;
    }
    b[i]=0;
}

int strlen(char *a)
{
    int i;
    i=0;
    while (a[i]!=0) i++;
    return i;
}

int clone(void (*function)(void), void *stack){
    int out;
    __asm__ (
        "movl $19, %%eax;"
	    "movl %1, %%ebx;"
        "movl %2, %%ecx;"
	    "int $0x80;"
        "movl %%eax, %0;"
        :"=r" (out)
        :"r" (function),
	 "r" (stack)
	:"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

/* sems */
int sem_init(int n_sem, unsigned int value) {
    int out;
    __asm__ (
        "movl $21, %%eax;"
        "movl %1, %%ebx;"
        "movl %2, %%ecx;"
        "int $0x80;"
        "movl %%eax, %0;"
        :"=r" (out)
        :"r" (n_sem)
        ,"r" (value)
        :"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

int sem_wait(int n_sem) {
    int out;
    __asm__ (
        "movl $22, %%eax;"
        "movl %1, %%ebx;"
        "int $0x80;"
        "movl %%eax, %0;"
        :"=r" (out)
        :"r" (n_sem)
        :"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

int sem_signal(int n_sem) {
    int out;
    __asm__ (
        "movl $23, %%eax;"
        "movl %1, %%ebx;"
        "int $0x80;"
        "movl %%eax, %0;"
        :"=r" (out)
        :"r" (n_sem)
        :"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

int sem_destroy(int n_sem) {
    int out;
    __asm__ (
        "movl $24, %%eax;"
        "movl %1, %%ebx;"
        "int $0x80;"
        "movl %%eax, %0;"
        :"=r" (out)
        :"r" (n_sem)
        :"%eax"
        );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}

void *sbrk(int increment) {
    void * out;
    __asm__ (
        "pushl %%ebx;"
        "movl $45, %%eax;"
        "movl %1, %%ebx;"
        "int $0x80;"
        "movl %%eax, %0;"
        "popl %%ebx;"
        :"=r" (out)
        :"r" (increment)
        :"%eax"
        );
    if(out<0) {
        seterrno(-(int)out);
        out = -1;
    }
    return out;
}

int read(int fd, char * buf, int count)
{
    int out;
    __asm__ (
             "pushl %%ebx;"
             "movl $3, %%eax;"
             "movl %1, %%ebx;"
             "movl %2, %%ecx;"
             "movl %3, %%edx;"
             "int $0x80;"
             "movl %%eax, %0;"
             "popl %%ebx;"
             :"=r" (out)
             :"r" (fd)
             ,"r" (buffer)
             ,"r" (size)
             :"%eax"
             );
    if(out<0) {
        seterrno(-out);
        out = -1;
    }
    return out;
}
