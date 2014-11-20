/*
* sys.c - Syscalls implementation
*/
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

int pids = 2;

int check_fd(int fd, int permissions)
{
if (fd!=1) return -9; /*EBADF*/
if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
    return 0;
}

int sys_ni_syscall()
{
    update_stats_user_to_system(current());
    update_stats_system_to_user(current());
return -38; /*ENOSYS*/
}

int sys_getpid()
{
    update_stats_user_to_system(current());
    update_stats_system_to_user(current());
    return current()->PID;
}

int sys_get_stats(int pid, struct stats *st)
{
    if(!access_ok(VERIFY_WRITE,st,sizeof(struct stats)))
        return -EFAULT;
    if(pid<0)
        return -EINVAL;
    if(pid>=0 && access_ok(VERIFY_WRITE,st,sizeof(struct stats))) {
        update_stats_user_to_system(current());
        if(current()->PID == pid) {
            copy_to_user(&current()->statistics,st,sizeof(struct stats));
            update_stats_system_to_user(current());
            return 0;
        }else{
            struct list_head * e;
            int i;
            for(i=0; i<NR_TASKS; ++i) {
                struct task_struct * t = (union task_union *)&(task[i].task);
                if(t->PID==pid) {
                    copy_to_user(&t->statistics,st,sizeof(struct stats));
                    update_stats_system_to_user(current());
                    return 0;
                }
            }
        }
        update_stats_system_to_user(current());
    }
    return -ESRCH;
}

int ret_from_fork() {
    update_stats_system_to_user(current());
    return 0;
}

int sys_fork() {
    update_stats_user_to_system(current());

    if(list_empty(&freequeue)) {
        update_stats_system_to_user(current());
        return -ENOMEM;
    }

    struct task_struct * actualTask = current();
    union task_union * actualUnion = (union task_union *) actualTask;

    struct list_head * e = list_first( &freequeue );
    struct task_struct * childTask = list_head_to_task_struct(e);

    list_del(e);

    union task_union * childUnion = (union task_union *) childTask;
    copy_data(actualUnion, childUnion, sizeof(union task_union));

    allocate_DIR(childTask);

    int pag;
    int new_ph_pag;
    int ph_pages[NUM_PAG_DATA];
    for (pag=0;pag<NUM_PAG_DATA;pag++){
        ph_pages[pag]=alloc_frame();
        if(ph_pages[pag]==-1) {
            int i;
            for (i = pag-1; i >= 0; --i) {
                free_frame(ph_pages[i]);
            }
            update_stats_system_to_user(current());
            return -EAGAIN;
        }
    }

    page_table_entry * childPT = get_PT(childTask);

    page_table_entry * parentPT = get_PT(actualTask);

    for (pag=0;pag<NUM_PAG_KERNEL+NUM_PAG_CODE;pag++){
        set_ss_pag(childPT, pag, get_frame(parentPT, pag));
    }

    for (pag=0; pag<NUM_PAG_DATA; pag++) {
        set_ss_pag(childPT, NUM_PAG_KERNEL+NUM_PAG_CODE+pag, ph_pages[pag]);
        set_ss_pag(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA+pag, ph_pages[pag]);
        copy_data(
            (NUM_PAG_KERNEL+NUM_PAG_CODE+pag)*PAGE_SIZE,
            (NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA+pag)*PAGE_SIZE,
            PAGE_SIZE
            );
        del_ss_pag(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA+pag);
    }

    set_cr3(actualTask->dir_pages_baseAddr);

    childTask->PID = pids;
    ++pids;

    unsigned int ebp;
    __asm__(
        "movl %%ebp, %0" :
        "=r" (ebp)
        );

    unsigned int dif = (ebp-(unsigned int)&actualUnion->stack[0])/4;
    childUnion->stack[dif] = &ret_from_fork;
    childUnion->stack[dif-1] = 0;
    childTask->kernel_esp = &(childUnion->stack[dif-1]);

    list_add_tail(&(childTask->list),&readyqueue);

    reset_stats(childTask);

    update_stats_system_to_user(current());

    return childTask->PID;
}

void sys_exit() {
    update_stats_user_to_system(current());
    /* liberamos semaphores;
    se puede optimizar? p ej. lista de semaforos en task_struct y list_head en sem_struct.
    Y cada vez que sem_init, entonces list_add_tail(&list_head, &current()->semaphores).
    Aun así, como solo hay 20 semáforos, creo que nos podemos permitir recorrerlos 1 a 1. */
    int i;
    for(i=0; i<NR_SEMAPHORES; ++i) {
        struct sem_struct * s = &semaphore[i];
        if(s->owner == current()->PID) {
            sys_sem_destroy(i);
        }
    }
    current()->PID = -1;
    free_user_pages(current());
    schedule_from_exit();
}

int sys_write(int fd, char* buffer, int size)
{
    update_stats_user_to_system(current());
    int written = 0;
    int err = check_fd(fd, ESCRIPTURA);
    if (err < 0) {
        update_stats_system_to_user(current());
        return err;
    }
    if (buffer == NULL) {
        seterrno(EFAULT);
        update_stats_system_to_user(current());
        return -EFAULT;
    }
    if (size < 0) {
        seterrno(EINVAL);
        update_stats_system_to_user(current());
        return -EINVAL;
    }
    if (!access_ok(VERIFY_READ,buffer,size)) {
        update_stats_system_to_user(current());
        return -1;
    }
    if(size>256) {
        char nbuff[256];
        while(size>256) {
            err = copy_from_user(buffer, &nbuff, 256);
            if (err < 0) {
                update_stats_system_to_user(current());
                return err;
            }
            size-=256;
            buffer+=256;
	    int res = sys_write_console(nbuff, 256);
            if(res >= 0) written+=res;
	    else return res;
        }
    }
    char nbuff[size];
    err = copy_from_user(buffer, &nbuff, size);
    if (err < 0) {
        update_stats_system_to_user(current());
        return err;
    }
    int res = sys_write_console(nbuff, size);
    if(res >= 0)written+= res;
    else return res;
    update_stats_system_to_user(current());
    return written;
}

int sys_clone(void (*funcion)(void), void *stack){
     update_stats_user_to_system(current());

    if(list_empty(&freequeue)) {
        update_stats_system_to_user(current());
        return -ENOMEM;
    }

    struct task_struct * actualTask = current();
    union task_union * actualUnion = (union task_union *) actualTask;

    struct list_head * e = list_first( &freequeue );
    struct task_struct * childTask = list_head_to_task_struct(e);

    list_del(e);

    union task_union * childUnion = (union task_union *) childTask;

    copy_data(actualUnion, childUnion, sizeof(union task_union));

    allocate_DIR(childTask);

    page_table_entry * childPT = get_PT(childTask);

    page_table_entry * parentPT = get_PT(actualTask);

    childTask->PID = actualTask->PID;

    unsigned int ebp;
    __asm__(
        "movl %%ebp, %0" :
        "=r" (ebp)
        );

    unsigned int dif = (ebp-(unsigned int)&actualUnion->stack[0])/4;
    childUnion->stack[dif] = &ret_from_fork;
    childUnion->stack[dif-1] = 0;
    childTask->kernel_esp = &(childUnion->stack[dif-1]);

    list_add_tail(&(childTask->list),&readyqueue);

    reset_stats(childTask);

    update_stats_system_to_user(current());

    return childTask->PID;
}

/* sems */
int sys_sem_init(int n_sem, unsigned int value) {
    int i;
    if(n_sem<0 || n_sem>=NR_SEMAPHORES)
        return -EINVAL;
    struct sem_struct * s = &semaphore[n_sem];
    if(s->owner<0) { // libre
        s->count = value;
        s->owner = current()->PID;
        return 0;
    }else{
        return -EBUSY;
    }
}

int sys_sem_wait(int n_sem) {
    if(n_sem<0 || n_sem>=NR_SEMAPHORES)
        return -EINVAL;

    struct sem_struct * s = &semaphore[n_sem];
    if(s->owner < 0)
        return -EINVAL;

    if(s->count<=0) {
        update_process_state_rr(current(), &s->blocked);
        sched_next_rr();
        if(s->owner<0) // se lo han cargado
            return -1;
    }else{
        --(s->count);
    }

    return 0;

}

int sys_sem_signal(int n_sem) {

    if(n_sem<0 || n_sem>=NR_SEMAPHORES)
        return -EINVAL;

    struct sem_struct * s = &semaphore[n_sem];
    if(s->owner < 0)
        return -EINVAL;
    
    if(list_empty(&s->blocked)) {
        s->count++;
    }else{
        struct list_head * e = list_first( &s->blocked );
        list_del(e);
        update_process_state_rr(list_head_to_task_struct(e), &readyqueue);
    }
    return 0;
}

int sys_sem_destroy(int n_sem) {
    if(n_sem<0 || n_sem>=NR_SEMAPHORES)
        return -EINVAL;

    struct sem_struct * s = &semaphore[n_sem];
    if(s->owner < 0)
        return -EINVAL;

    if(current()->PID == s->owner) {
        s->count = 0;
        s->owner = -1;
        if(!list_empty(&s->blocked)) {
            struct list_head * e = list_first( &s->blocked );
            list_del(e);
            list_add_tail(e, &readyqueue);
            list_for_each(e, &s->blocked) {
                list_del(e);
                list_add_tail(e, &readyqueue);
            }
        }
        return 0;
    }else{
        return -EPERM;
    }
}