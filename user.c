#include <libc.h>

char buff[24];

int pid;

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

    runjp();





    /*while(1) {
        // TEST TASK_SWITCH + GETPID FUNCIONA
        char * buff = "\nejecutando task init. pid: ";
        write(1, buff,strlen(buff));
        char buffer[1];
        itoa(getpid(),buffer);
        write(1, buffer, strlen(buffer));
        buff = "\n";
        write(1, buff, strlen(buff));
    }*/
}
