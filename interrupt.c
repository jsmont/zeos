/*
* interrupt.c -
*/
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <sched.h>
#include <hardware.h>
#include <io.h>

#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

char char_map[] =
{
    '\0','\0','1','2','3','4','5','6',
    '7','8','9','0','\'','¡','\0','\0',
    'q','w','e','r','t','y','u','i',
    'o','p','`','+','\0','\0','a','s',
    'd','f','g','h','j','k','l','ñ',
    '\0','º','\0','ç','z','x','c','v',
    'b','n','m',',','.','-','\0','*',
    '\0','\0','\0','\0','\0','\0','\0','\0',
    '\0','\0','\0','\0','\0','\0','\0','7',
    '8','9','-','4','5','6','+','1',
    '2','3','0','\0','\0','\0','<','\0',
    '\0','\0','\0','\0','\0','\0','\0','\0',
    '\0','\0'
};

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
/***********************************************************************/
/* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
/* ***************************                                         */
/* flags = x xx 0x110 000 ?????                                        */
/*         |  |  |                                                     */
/*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
/*         |   \ DPL = Num. higher PL from which it is accessible      */
/*          \ P = Segment Present bit                                  */
/***********************************************************************/
    Word flags = (Word)(maxAccessibleFromPL << 13);
flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

    idt[vector].lowOffset       = lowWord((DWord)handler);
    idt[vector].segmentSelector = __KERNEL_CS;
    idt[vector].flags           = flags;
    idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
/***********************************************************************/
/* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
/* ********************                                                */
/* flags = x xx 0x111 000 ?????                                        */
/*         |  |  |                                                     */
/*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
/*         |   \ DPL = Num. higher PL from which it is accessible      */
/*          \ P = Segment Present bit                                  */
/***********************************************************************/
    Word flags = (Word)(maxAccessibleFromPL << 13);

//flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
/* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
the system calls will be thread-safe. */
flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

    idt[vector].lowOffset       = lowWord((DWord)handler);
    idt[vector].segmentSelector = __KERNEL_CS;
    idt[vector].flags           = flags;
    idt[vector].highOffset      = highWord((DWord)handler);
}


void setIdt()
{
/* Program interrups/exception service routines */
    idtR.base  = (DWord)idt;
    idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;

    set_handlers();

/* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */

    setInterruptHandler(32, clock_handler, 0);
    setInterruptHandler(33, keyboard_handler, 0);
    setTrapHandler(0x80, system_call_handler, 3);
    set_idt_reg(&idtR);
}

void clock_routine() {
    update_stats_user_to_system(current());
    schedule();
    zeos_show_clock();
    update_stats_system_to_user(current());
}

void keyboard_routine()
{
    update_stats_user_to_system(current());
    //printc_xy(1, 22, 'K');
    unsigned char input = inb(0x60);
    unsigned char is_break = input >> 7;
    unsigned char scan_code = input & 0x7F;
    if (!is_break)
    {
	//printc_xy(1, 22, 'C');
        unsigned char key_char = ' ';
        if (scan_code < 128)
        {
            key_char = char_map[scan_code];
        }
  //      printc_xy(1, 22, 'B');
        if(buffer_size() < BUFFER_SIZE)
        {
//printc_xy(1, 22, 'B');
            push(key_char);

        }
    
  //      printc_xy(1, 22, 'B');
        if(!list_empty(&keyboardqueue))
        {
//printc_xy(1, 22, 'S');
            struct task_struct * to_unblock = list_head_to_task_struct(list_first(&keyboardqueue));
//printc_xy(1, 22, 'S');
	update_process_state_rr(to_unblock,&readyqueue);
//printc_xy(1, 22, 'S');
                //sched_next_rr();
                
        }
    }
    update_stats_system_to_user(current());
}
