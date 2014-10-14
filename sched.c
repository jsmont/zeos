/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

struct task_struct *idle_task;

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* task array */

struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}

extern struct list_head blocked;


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t)
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t)
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t)
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos];

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_freequeue() {
    INIT_LIST_HEAD( &freequeue );
    int i;
    for(i=0; i<NR_TASKS ; ++i) {
        list_add(&task[i].task.list, &freequeue );
    }
}

void init_idle (void)
{
    struct list_head * e = list_first( &freequeue );
    struct task_struct * t = list_head_to_task_struct(e);
    t->PID = 0;
    allocate_DIR(t);

    union task_union * tu = (union task_union *) t;
    tu->stack[KERNEL_STACK_SIZE-2] = 0;
    tu->stack[KERNEL_STACK_SIZE-1] = &cpu_idle;

    t->kernel_esp = (int) & (tu->stack[KERNEL_STACK_SIZE-2]);

    idle_task = t;

    list_del(e);
}

void init_task1(void)
{

}


void init_sched(){

}

struct task_struct* current()
{
  int ret_value;

  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

void task_switch(union task_union* new) {
   __asm__ ("pushl %esi\n\t"
             "pushl %edx\n\t"
             "pushl %ebx");
   inner_task_switch(new);
   __asm__ ("popl %ebx\n\t"
             "popl %edi\n\t"
             "popl %esi");
}

void inner_task_switch(union task_union* t) {
  // PÀG. 49 Documentacio.pdf, PÀG. 30 Slide tema 4.pdf
  struct task_struct * actualTask = current();
  __asm__ (
    "pushl %ebp\n\t"
    "movl %esp, %ebp"
  );
  int ebp;
  __asm__(
    "movl %%ebp, %0" :
    "=r" (ebp)
  );
  actualTask->kernel_esp = ebp;
  struct task_struct * newTask = &(t->task);
  tss.esp0 = newTask->kernel_esp;
  set_cr3(get_DIR(newTask));
  __asm__(
    "popl %ebp\n\t"
    "ret"
  );
}

