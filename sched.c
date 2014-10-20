/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <stats.h>

struct task_struct *idle_task;
struct task_struct *init_task;

unsigned int tics;

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
        // TEST TASK_SWITCH + GETPID FUNCIONA
        /*char * chivato = "\nejecutando task idle. pid: ";
        printk(chivato);
        char buffer[1];
        itoa( (struct task_struct *)current()->PID, buffer);
        printk(buffer);
        chivato = "\n";
        printk(chivato);*/
    }
}

void init_idle (void)
{
    struct list_head * e = list_first( &freequeue );
    idle_task = list_head_to_task_struct(e);
    list_del(e);
    idle_task->PID = 0;
    allocate_DIR(idle_task);

    union task_union * tu = (union task_union *) idle_task;
    tu->stack[KERNEL_STACK_SIZE-2] = 0;
    tu->stack[KERNEL_STACK_SIZE-1] = &cpu_idle;

    idle_task->kernel_esp = (unsigned int) & (tu->stack[KERNEL_STACK_SIZE-2]);

    set_quantum(idle_task,1);
    reset_stats(idle_task);
}

void init_task1(void)
{
    struct list_head * e = list_first( &freequeue );
    init_task = list_head_to_task_struct(e);
    list_del(e);

    init_task->PID = 1;

    allocate_DIR(init_task);
    set_user_pages(init_task);

    union task_union * tu = (union task_union *) init_task;

    tss.esp0 = &(tu->stack[KERNEL_STACK_SIZE]);
    set_cr3(get_DIR(init_task));

    set_quantum(init_task,10);
    reset_stats(init_task);
}

void init_sched(){
    tics = 1;
    INIT_LIST_HEAD( &freequeue );
    int i;
    for(i=0; i<NR_TASKS ; ++i) {
        list_add(&task[i].task.list, &freequeue );
    }

    INIT_LIST_HEAD( &readyqueue );
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

void task_switch(union task_union* newTask) {
   __asm__ __volatile__ ("pushl %esi\n\t"
             "pushl %edi\n\t"
             "pushl %ebx");
   inner_task_switch(newTask);
   __asm__ __volatile__("popl %ebx\n\t"
             "popl %edi\n\t"
             "popl %esi");
}

void inner_task_switch(union task_union* t) {
    // PÀG. 49 Documentacio.pdf, PÀG. 30 Slide tema 4.pdf
    struct task_struct * newTask = &(t->task);

    tss.esp0 = (unsigned int)(newTask->kernel_esp);

    set_cr3(get_DIR(newTask));

    unsigned int ebp;
    __asm__ __volatile__(
        "movl %%ebp, %0" :
        "=r" (ebp)
    );

    struct task_struct * actualTask = current();
    actualTask->kernel_esp = ebp;

    __asm__ __volatile__(
        "movl %0, %%esp" ::
        "r" (newTask->kernel_esp)
    );

    __asm__ __volatile__(
        "popl %ebp\n\t"
        "ret"
    );
}

void schedule_from_exit() {
    update_process_state_rr(current(),&freequeue);
    sched_next_rr();
}

void sched_next_rr() {
    if(!list_empty(&readyqueue)) {
        struct list_head * e = list_first( &readyqueue );
        struct task_struct * t = list_head_to_task_struct(e);
        update_process_state_rr(t,NULL);
        update_stats_ready_to_system(t);
        tics = get_quantum(t);
        update_stats_system_to_user(t);
        task_switch((union task_union *)t);
    }else{
        tics = get_quantum(idle_task);
        update_stats_system_to_user(idle_task);
        task_switch((union task_union *)idle_task);
    }
}

void update_process_state_rr(struct task_struct *t, struct list_head *dest) {
    if(dest==NULL) {
        list_del(&t->list);
    }else{
        list_add_tail(&t->list, dest);
    }
}

int needs_sched_rr() {
    if(tics==0 && !list_empty(&readyqueue))
        return 1;
    return 0;
}

void update_sched_data_rr() {
    if(tics>0)
        tics--;
}

void schedule() {
    update_sched_data_rr();
    if(needs_sched_rr()) {
        struct task_struct * oldT = current();
        if(oldT->PID > 0) {
            update_stats_system_to_ready(oldT);
            update_process_state_rr(oldT,&readyqueue);
        }
        sched_next_rr();
    }else if(tics==0){
        tics = get_quantum(current());
    }
}

int get_quantum (struct task_struct *t) {
    return t->quantum;
}

void set_quantum (struct task_struct *t, int new_quantum) {
    t->quantum = new_quantum;
}

void update_stats_user_to_system() {
    struct stats * actualStats = &(current()->statistics);
    actualStats->user_ticks += get_ticks()-actualStats->elapsed_total_ticks;
    actualStats->elapsed_total_ticks = get_ticks();
    actualStats->remaining_ticks = tics;
}

void update_stats_system_to_user(struct task_struct * t) {
    struct stats * actualStats = &(t->statistics);
    actualStats->system_ticks += get_ticks()-actualStats->elapsed_total_ticks;
    actualStats->elapsed_total_ticks = get_ticks();
    actualStats->remaining_ticks = tics;
}

void update_stats_system_to_ready(struct task_struct * t) {
    struct stats * actualStats = &(t->statistics);
    actualStats->user_ticks += get_ticks()-actualStats->elapsed_total_ticks;
    actualStats->elapsed_total_ticks = get_ticks();
}

void update_stats_ready_to_system(struct task_struct * t) {
    struct stats * actualStats = &(t->statistics);
    actualStats->user_ticks += get_ticks()-actualStats->elapsed_total_ticks;
    actualStats->elapsed_total_ticks = get_ticks();
    actualStats->remaining_ticks = tics;
    actualStats->total_trans++;
}

void reset_stats(struct task_struct * t) {
    struct stats * actualStats = &(t->statistics);
    actualStats->user_ticks = 0;
    actualStats->system_ticks = 0;
    actualStats->blocked_ticks = 0;
    actualStats->ready_ticks = 0;
    actualStats->elapsed_total_ticks = 0;
    actualStats->total_trans = 0; /* Number of times the process has got the CPU: READY->RUN transitions */
    actualStats->remaining_ticks = 0;
}