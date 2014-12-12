#include <errno.h>

int errno;

void seterrno(int val) {
    errno = val;
}

int geterrno() {
    return errno;
}
