#include <libc.h>

char buff[24];

int pid;

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

    runjp();

    while(1) {
        ;
    }


/*
    int local = 200; // local declarada
    int a = fork(); // fork
    if(a>0) {
        write(1,(char *)"\nSOY PADRE (1).",14);
    }else if(a<0) {
        write(1,(char *)"\nSOY ERROR (1).",14);
    }else if(a==0){
        write(1,(char *)"\nSOY HIJO (1).",13);
        exit();
    }

    while(1) {
        if(a>0) {
            int i = 0;
            while(i<9999999) { // aseguramos se ejecutará después de matar hijo
                ++i;
            }
            write(1,(char *)"\nVALOR local: ",14); // debería dar 200
            char buff[4];
            itoa(local,buff);
            write(1,buff,strlen(buff));

            //int pidpadre = getpid();
            //char buffa[4];
            //itoa(pidpadre,buffa);
            //write(1,(char *)"\nGETPID:",8);
            //write(1,buffa,strlen(buffa));

        }else if(a<0) {
            write(1,(char *)"\nSOY ERROR.",11);
        }else if(a==0){
            write(1,(char *)"\nSOY HIJO.",10);
        }
    }
*/

}
