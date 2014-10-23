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

 int pids = 2; // next PID to use

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
    if(pid>=0) {
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
                    copy_to_user(&current()->statistics,st,sizeof(struct stats));
                    update_stats_system_to_user(current());
                    return 0;
                }
            }
        }
        update_stats_system_to_user(current());
    }
    return -1;
}

int ret_from_fork() {
    update_stats_system_to_user(current());
    return 0;
}

int sys_fork() {
    update_stats_user_to_system(current());
    //  Get a free task_struct for the process. If there is no space for a new process,
    //  an error will be returned.

    if(list_empty(&freequeue))
        return -ENOMEM; // no hay espacio para contener nuevo proceso

    //  Inherit system data: copy the parent’s task_union to the child. Determine
    //  whether it is necessary to modify the page table of the parent to access the
    //  child’s system data. The copy_data function can be used to copy

    struct task_struct * actualTask = current();
    union task_union * actualUnion = (union task_union *) actualTask;

    struct list_head * e = list_first( &freequeue );
    struct task_struct * childTask = list_head_to_task_struct(e);

    list_del(e);

    union task_union * childUnion = (union task_union *) childTask;

    copy_data(actualUnion, childUnion, sizeof(union task_union));

    //  Initialize field dir_pages_baseAddr with a new directory to store the
    //  process address space using the allocate_DIR routine.

    allocate_DIR(childTask);

    //  Search physical pages in which to map logical pages for data+stack of the
    //  child process (using the alloc_frames function). If there is no enough free
    //  pages, an error will be return.

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

    //  Inherit user data:
    //
    //      Create new address space: Access page table of the child process through the
    //      directory field in the task_struct to initialize it (get_PT routine can be used):

    page_table_entry * childPT = get_PT(childTask);

    //          Page table entries for the system code and data and for the user code can be a
    //          copy of the page table entries of the parent process (they will be shared)

    page_table_entry * parentPT = get_PT(actualTask);

    for (pag=0;pag<NUM_PAG_KERNEL+NUM_PAG_CODE;pag++){
        set_ss_pag(childPT, pag, get_frame(parentPT, pag));
    }

    //          Page table entries for the user data+stack have to point to new allocated pages
    //          which hold this region

    /*for (pag=0; pag<NUM_PAG_DATA; pag++) {
        set_ss_pag(childPT, NUM_PAG_KERNEL+NUM_PAG_CODE+pag, ph_pages[pag]);
    }*/

    //  Copy the user data+stack pages from the parent process to the child process. The
    //  child’s physical pages cannot be directly accessed because they are not mapped in
    //  the parent’s page table. In addition, they cannot be mapped directly because the
    //  logical parent process pages are the same. They must therefore be mapped in new
    //  entries of the page table temporally (only for the copy). Thus, both pages can be
    //  accessed simultaneously as follows:
    //
    //      Use temporal free entries on the page table of the parent. Use the set_ss_pag and
    //      del_ss_pag functions.
    //
    //      Copy data+stack pages
    //
    //      Free temporal entries in the page table and flush the TLB to really disable the
    //      parent process to access the child pages.

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

    // Assign a new PID to the process. The PID must be different from its position in the
    // task_array table.

    childTask->PID = pids;
    ++pids;

    //  Initialize the fields of the task_struct that are not common to the child.
    //
    //      Think about the register or registers that will not be common in the returning of the
    //      child process and modify its content in the system stack so that each one receive its
    //      values when the context is restored.
    //
    //  Prepare the child stack with a content that emulates the result of a call to task_switch.
    //  This process will be executed at some point by issuing a call to task_switch. Therefore
    //  the child stack must have the same content as the task_switch expects to find, so it will
    //  be able to restore its context in a known position. The stack of this new process must
    //  be forged so it can be restored at some point in the future by a task_switch. In fact this
    //  new process has to:
    //
    //      a) restore its hardware context
    //
    //      b) continue the execution of the user process, so you must create a routine
    //      ret_from_fork which does exactly this. And use it as the restore point like
    //      in the idle process initialization 4.4.

    unsigned int ebp;
    __asm__(
        "movl %%ebp, %0" :
        "=r" (ebp)
    );

    unsigned int dif = (ebp-(unsigned int)&actualUnion->stack[0])/4;
    childUnion->stack[dif] = &ret_from_fork;
    childUnion->stack[dif-1] = 0;
    childTask->kernel_esp = &(childUnion->stack[dif-1]);

    //  Insert the new process into the ready list: readyqueue. This list will contain all processes
    //  that are ready to execute but there is no processor to run them.

    list_add_tail(&(childTask->list),&readyqueue);

    reset_stats(childTask);

    update_stats_system_to_user(current());

    // Return the pid of the child process.
    return childTask->PID;
}

void sys_exit() {
    update_stats_user_to_system(current());
    free_user_pages(current());
    /*int i;
    page_table_entry * p = get_PT(current());
    for(i=NUM_PAG_KERNEL+NUM_PAG_CODE; i<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; ++i) {
        unsigned int frame = get_frame(p, NUM_PAG_KERNEL+NUM_PAG_CODE+i);
        free_frame(frame);
        del_ss_pag(p,NUM_PAG_KERNEL+NUM_PAG_CODE+i);
    }*/
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
        written+=sys_write_console(nbuff, 256);
    }
}
char nbuff[size];
err = copy_from_user(buffer, &nbuff, size);
if (err < 0) {
    update_stats_system_to_user(current());
    return err;
}
written+=sys_write_console(nbuff, size);
update_stats_system_to_user(current());
return written;
}
