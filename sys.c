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
    printc_xy(2, 22, fd+48);
    printc_xy(3, 22, permissions+48);
    if ((fd != 0) && (fd != 1)) return -9;
    if ((fd == 0) && (permissions != LECTURA)) return -13;
    if ((fd == 1) && (permissions!=ESCRIPTURA)) return -13; /*EACCES*/
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
    
    unsigned int actualBreak = heap_structs[search_DIR(current())].program_break;
    unsigned int actualPage = (actualBreak) >> 12;
    int nHeapPages = actualPage-L_USER_HEAP_P0+1;
    if(nHeapPages<0) // heap no inicializado
        nHeapPages = 0;
    unsigned int totalPages = NUM_PAG_DATA+nHeapPages;
    
    int pag;
    int ph_pages[totalPages];
    for (pag=0;pag<totalPages;pag++){
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
    
    /* kernel + código */
    for (pag=0;pag<NUM_PAG_KERNEL+NUM_PAG_CODE;pag++){
        set_ss_pag(childPT, pag, get_frame(parentPT, pag));
    }
    
    /* datos + heap */
    for (pag=0; pag<totalPages; pag++) {
        set_ss_pag(childPT, NUM_PAG_KERNEL+NUM_PAG_CODE+pag, ph_pages[pag]);
        set_ss_pag(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+totalPages+pag, ph_pages[pag]);
        copy_data(
                  (NUM_PAG_KERNEL+NUM_PAG_CODE+pag)*PAGE_SIZE,
                  (NUM_PAG_KERNEL+NUM_PAG_CODE+totalPages+pag)*PAGE_SIZE,
                  PAGE_SIZE
                  );
        del_ss_pag(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+totalPages+pag);
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
    
    cont_dir[search_DIR(childTask)] = 1;
    
    updateProgramBreakAllProcesses(actualBreak,childTask);
    
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
    --cont_dir[search_DIR(current())];
    if (cont_dir[search_DIR(current())] <= 0) {
        free_user_pages(current());
    }
    
    current()->PID = -1;
    
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
    
    //Comprovacions de seguretat
    if (!access_ok(VERIFY_WRITE, stack,4) || !access_ok(VERIFY_READ, funcion, 4)){
        update_stats_user_to_system(current());
        return -EFAULT;
    }
    
    //Comprovem si hi ha processos disponibles
    if(list_empty(&freequeue)) {
        update_stats_system_to_user(current());
        return -ENOMEM;
    }
    
    //Creem la nova tasca
    union task_union * actualUnion = (union task_union *) current();
    
    struct list_head * e = list_first( &freequeue );
    struct task_struct * childTask = list_head_to_task_struct(e);
    
    list_del(e);
    
    
    union task_union * childUnion = (union task_union *) childTask;
    copy_data(actualUnion, childUnion, sizeof(union task_union));
    
    //Assignem el nou PID i incrementem el contador del directori
    childTask->PID = pids;
    ++pids;
    ++cont_dir[search_DIR(current())];
    
    int current_ebp;
    __asm__ __volatile__(
                         "mov %%ebp,%0;"
                         : "=r" (current_ebp)
                         );
    unsigned int pos_ebp = ((unsigned int)current_ebp-(unsigned int)current())/4;
    
    
    //
    childUnion->task.kernel_esp = (unsigned int)&childUnion->stack[pos_ebp-1];
    
    childUnion->stack[pos_ebp-1] = 0;
    
    
    /* @ retorn estàndard: Restore ALL + iret */
    childUnion->stack[pos_ebp] = (unsigned int)&ret_from_fork;
    /* Modificació del ebp amb la @ de la stack */
    childUnion->stack[pos_ebp+7] = (unsigned int)stack;
    
    // |	eip	|
    // |	es	|
    // |eflags|
    // |	esp	|
    // |	ss	|
    
    /* Modificació del registre eip que restaurarà el iret */
    childUnion->stack[pos_ebp+13] = (unsigned int)funcion;
    /* Modificació del registre esp per fer a la @stack: push   %ebp 	*/
    childUnion->stack[pos_ebp+16] = (unsigned int)stack;
    
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

void *sys_sbrk(int increment) {
    
    struct task_struct * actualTask = current();
    page_table_entry * actualPT = get_PT(actualTask);
    
    unsigned int actualBreak = heap_structs[search_DIR(current())].program_break;
    
    if(actualBreak==0) { // ups
        int frame = alloc_frame();
        if(frame==-1) {
            free_frame(frame);
            return -EAGAIN;
        }
        set_ss_pag(actualPT, L_USER_HEAP_P0, frame);
        updateProgramBreakAllProcesses(L_USER_HEAP_START, actualTask);
    }
    
    actualBreak = heap_structs[search_DIR(current())].program_break;
    if(increment>0) {
        // asignamos espacio.
        unsigned int actualPage = (actualBreak) >> 12; // pagina actual heap
        unsigned int finalPage = (actualBreak+increment) >> 12; // pagina heap después del incremento
        if(finalPage-L_USER_HEAP_P0 > MAX_HEAP_PAGES) // solo dejamos 64 paginas de heap max.
            return -1;
        
        int ph_pages[finalPage-actualPage];
        int i;
        for (i = 0; i < finalPage-actualPage; ++i) { // reservamos las páginas!
            ph_pages[i]=alloc_frame();
            if(ph_pages[i]==-1) {
                int j;
                for (j = i-1; j >= 0; --j) {
                    free_frame(ph_pages[j]);
                }
                return -EAGAIN;
            }
        }
        // reservadas, queda asignarlas
        for (i = 0; i < finalPage-actualPage; ++i) {
            set_ss_pag(actualPT, actualPage+1+i, ph_pages[i]);
        }
        updateProgramBreakAllProcesses(actualBreak+increment, actualTask);
        return actualBreak; // devolvemos a partir de donde hemos asignado
    }else if(increment<0) {
        unsigned int actualPage = (actualBreak) >> 12; // pagina actual
        unsigned int finalPage = (actualBreak+increment) >> 12; // pagina después del incremento
        
        if(finalPage < L_USER_HEAP_P0) { // se pasa de frenada
            updateProgramBreakAllProcesses(L_USER_HEAP_START, actualTask);
            return -EINVAL;
        }
        
        int i;
        for (i = actualPage; i > finalPage; --i) {
            unsigned int frame = get_frame(actualPT, i);
            free_frame(frame);
            del_ss_pag(actualPT, i);
        }
        updateProgramBreakAllProcesses(actualBreak+increment, actualTask);
        return actualBreak; // devolvemos antiguo break
    }else if(increment==0){
        // devolvemos pointer actual
        return actualBreak;
    }
}

int sys_read_keyboard(char * buf, int count)
{
    
    current()->read_pending = count;
    
    if (!list_empty(&keyboardqueue))
    {
        struct list_head * elem = &current()->list;
        list_del(elem);
        list_add_tail(elem, &keyboardqueue);
        sched_next_rr();
    }
    
    int current_read = 0;
    unsigned int * current_count = &current()->read_pending;
    
    while (*current_count > 0)
    {
        {
            if (buffer.start > buffer.end)
            {
                int len_a = &buffer.buffer[BUFFER_SIZE] - buffer.start;
                if (copy_to_user(buffer.start, buf + current_read, len_a) < 0)
                {
                    return -1;
                }
                
                *current_count -= len_a;
                current_read += len_a;
                
                
                
                int len_b = buffer.end - &buffer.buffer[0];
                char * start = &buffer.buffer[0];
                if (copy_to_user(start, buf + len_a + current_read, len_b) < 0)
                {
                    
                    return -1;
                }
                *current_count -= len_b;
                current_read += len_b;
                
                pop_i(len_a + len_b);
            }
            
            else
            {
                int size = buffer_size();
                if (copy_to_user(buffer.start, buf + current_read, size) < 0)
                {
                    return -1;
                }
                pop_i(size);
                *current_count -= size;
                current_read += size;
            }
        }
        
        if (*current_count > 0)
        {
            struct list_head * elem = &current()->list;
            list_del(elem);
            list_add_tail(elem, &keyboardqueue);
            sched_next_rr();
        }
        
    }
    return current_read;
}

int sys_read(int fd, char * buf,int count){
    int ch_fd = check_fd(fd, LECTURA);
    
    if (ch_fd < 0)
    {
        return ch_fd;
    }
    if (count < 0)
    {
        return -EINVAL;
    }
    
    return sys_read_keyboard(buf, count);
}