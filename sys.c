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
	return -38; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int ret_from_fork() {
    return 0;
}

int sys_fork() {

    //  Get a free task_struct for the process. If there is no space for a new process,
    //  an error will be returned.

    if(list_empty(&freequeue))
    return -1; // no hay espacio para contener nuevo proceso

    //  Inherit system data: copy the parent’s task_union to the child. Determine
    //  whether it is necessary to modify the page table of the parent to access the
    //  child’s system data. The copy_data function can be used to copy

    struct task_struct * actualTask = current();
    union task_union * actualUnion = (union task_union *) actualTask;

    struct list_head * e = list_first( &freequeue );
    struct task_struct * childTask = list_head_to_task_struct(e);

    union task_union * childUnion = (union task_union *) childTask;

    copy_data(actualUnion, childUnion, sizeof(actualUnion));

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
    new_ph_pag=alloc_frame();
    if(new_ph_pag==-1)
      return -1; // error no hay physical pages disponibles. pendiente liberarlas??
    else
      ph_pages[pag] = new_ph_pag;
    }

    //  Inherit user data:
    //
    //      Create new address space: Access page table of the child process through the
    //      directory field in the task_struct to initialize it (get_PT routine can be used):

    page_table_entry * childPT = get_PT(childTask);

    //          Page table entries for the system code and data and for the user code can be a
    //          copy of the page table entries of the parent process (they will be shared)

    page_table_entry * parentPT = get_PT(actualTask);

    for (pag=NUM_PAG_KERNEL;pag<NUM_PAG_KERNEL+NUM_PAG_CODE;pag++){
        childPT[pag].entry = 0;
        childPT[pag].bits.pbase_addr = parentPT[pag].bits.pbase_addr;
        childPT[pag].bits.user = 1;
        childPT[pag].bits.present = 1;
    }

    //
    //          Page table entries for the user data+stack have to point to new allocated pages
    //          which hold this region

    for (pag=0; pag<NUM_PAG_DATA; pag++) {
        childPT[NUM_PAG_KERNEL+NUM_PAG_CODE+pag].entry = 0;
        childPT[NUM_PAG_KERNEL+NUM_PAG_CODE+pag].bits.pbase_addr = ph_pages[pag];
        childPT[NUM_PAG_KERNEL+NUM_PAG_CODE+pag].bits.user = 1;
        childPT[NUM_PAG_KERNEL+NUM_PAG_CODE+pag].bits.rw = 1;
        childPT[NUM_PAG_KERNEL+NUM_PAG_CODE+pag].bits.present = 1;
    }

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
        set_ss_pag(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA+pag, ph_pages[pag]);
        unsigned int physicalDataFrameParent = get_frame(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+pag);
        unsigned int physicalDataFrameChild = get_frame(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+pag);
        copy_data(physicalDataFrameParent, physicalDataFrameChild, PAGE_SIZE);
        del_ss_pag(parentPT, NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA+pag);
    }

    set_cr3(parentPT);

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

    list_add_tail(&childTask->list,&readyqueue);

    // Return the pid of the child process.

    return childTask->PID;
}

void sys_exit()
{
}

int sys_write(int fd, char* buffer, int size)
{
    int written = 0;
    int err = check_fd(fd, ESCRIPTURA);
    if (err < 0) return err;
    if (buffer == NULL) {
      seterrno(EFAULT);
      return -EFAULT;
  }
  if (size < 0) {
      seterrno(EINVAL);
      return -EINVAL;
  }
  if (!access_ok(VERIFY_READ,buffer,size)) return -1;
  if(size>256) {
      char nbuff[256];
      while(size>256) {
        err = copy_from_user(buffer, &nbuff, 256);
        if (err < 0) return err;
        size-=256;
        buffer+=256;
        written+=sys_write_console(nbuff, 256);
    }
}
char nbuff[size];
err = copy_from_user(buffer, &nbuff, size);
if (err < 0) return err;
written+=sys_write_console(nbuff, size);
return written;
}
