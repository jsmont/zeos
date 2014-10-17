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

int sys_fork()
{
  int PID=-1;

  if(list_empty(&freequeue))
    return -1; // no hay espacio para contener nuevo proceso

  struct task_struct * actualTask = current();
  union task_union * actualUnion = (union task_union *) actualTask;

  struct list_head * e = list_first( &freequeue );
  struct task_struct * childTask = list_head_to_task_struct(e);

  union task_union * childUnion = (union task_union *) childTask;

  childUnion->task = actualUnion->task;

  int pag;
  int new_ph_pag;
  int ph_pages[NUM_PAG_DATA];
  for (pag=0;pag<NUM_PAG_DATA;pag++){
    new_ph_pag=alloc_frame();
    if(new_ph_pag==-1)
      return -1; // error no hay physical pages disponibles
    else
      ph_pages[pag] = new_ph_pag;
  }

  page_table_entry * childPT = get_PT(childTask);
  page_table_entry * parentPT = get_PT(actualTask);

  for (pag=0;pag<NUM_PAG_CODE;pag++){
    childPT[pag].entry = 0;
    childPT[pag].bits.pbase_addr = parentPT[pag].bits.pbase_addr;
    childPT[pag].bits.user = 1;
    childPT[pag].bits.present = 1;
  }

  // hasta aquí transpa 48

  childTask->PID = pids;
  ++pids;

  return PID;
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
