/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <stats.h>
#include <mm_address.h>

#define NR_TASKS            10
#define NR_SEMAPHORES       20
#define KERNEL_STACK_SIZE	1024
#define BUFFER_SIZE 5

struct list_head freequeue;
struct list_head readyqueue;
struct list_head keyboardqueue;

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct task_struct {
  int PID;			/* Process ID */
  page_table_entry * dir_pages_baseAddr;
  struct list_head list;
  unsigned int kernel_esp;
  unsigned int quantum;
  unsigned int read_pending;
  struct stats statistics;
};

struct sem_struct {
  int count;
  struct list_head blocked;
  int owner;
};

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

struct circular_buffer {
    char * start;
    char * end;
    char size;
    char buffer[BUFFER_SIZE];
};

extern struct circular_buffer buffer;

extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union *task; /* Vector de tasques */
extern struct task_struct *idle_task;
extern struct task_struct *init_task;
extern int cont_dir[NR_TASKS];

extern struct sem_struct semaphores[NR_SEMAPHORES];
extern struct sem_struct *semaphore;

extern unsigned int tics;

#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

void init_sem(void);

void init_keyboard(void);

struct task_struct * current();

void task_switch(union task_union* new);

void inner_task_switch(union task_union* new);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

int get_quantum (struct task_struct *t);
void set_quantum (struct task_struct *t, int new_quantum);

void update_stats_user_to_system();
void update_stats_system_to_user();
void update_stats_system_to_ready();
void update_stats_ready_to_system();

void reset_stats(struct task_struct *t);

/*Gestió de buffer*/
int buffer_size();
void push(char c);
char pop();
void pop_i(int size);

#endif  /* __SCHED_H__ */
