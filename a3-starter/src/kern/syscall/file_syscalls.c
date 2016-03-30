/* BEGIN A3 SETUP */
/* This file existed for A1 and A2, but has been completely replaced for A3.
 * We have kept the dumb versions of sys_read and sys_write to support early
 * testing, but they should be replaced with proper implementations that 
 * use your open file table to find the correct vnode given a file descriptor
 * number.  All the "dumb console I/O" code should be deleted.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <copyinout.h>
#include <synch.h>
#include <file.h>

/* This special-case global variable for the console vnode should be deleted 
 * when you have a proper open file table implementation.
 */
//struct vnode *cons_vnode=NULL; 

/* This function should be deleted, including the call in main.c, when you
 * have proper initialization of the first 3 file descriptors in your 
 * open file table implementation.
 * You may find it useful as an example of how to get a vnode for the 
 * console device.
 */
// void dumb_consoleIO_bootstrap()
// {
//   int result;
//   char path[5];

//   /* The path passed to vfs_open must be mutable.
//    * vfs_open may modify it.
//    */

//   strcpy(path, "con:");
//   result = vfs_open(path, O_RDWR, 0, &cons_vnode);

//   if (result) {
//      // Tough one... if there's no console, there's not
//      // * much point printing a warning...
//      // * but maybe the bootstrap was just called in the wrong place
     
//     kprintf("Warning: could not initialize console vnode\n");
//     kprintf("User programs will not be able to read/write\n");
//     cons_vnode = NULL;
//   }
// }

/*
 * mk_useruio
 * sets up the uio for a USERSPACE transfer. 
 */
static
void
mk_useruio(struct iovec *iov, struct uio *u, userptr_t buf, 
	   size_t len, off_t offset, enum uio_rw rw)
{

	iov->iov_ubase = buf;
	iov->iov_len = len;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = offset;
	u->uio_resid = len;
	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}

/*
 * sys_open
 * just copies in the filename, then passes work to file_open.
 * You have to write file_open.
 * 
 */
int
sys_open(userptr_t filename, int flags, int mode, int *retval)
{
	char *fname;
	int result;

	if ( (fname = (char *)kmalloc(__PATH_MAX)) == NULL) {
		return ENOMEM;
	}

	result = copyinstr(filename, fname, __PATH_MAX, NULL);
	if (result) {
		kfree(fname);
		return result;
	}

	result =  file_open(fname, flags, mode, retval);
	kfree(fname);
	return result;
}

/* 
 * sys_close
 * You have to write file_close.
 */
int
sys_close(int fd)
{
	return file_close(fd);
}

/* 
 * sys_dup2
 * 
 */
int
sys_dup2(int oldfd, int newfd, int *retval)
{
    if ((oldfd < 0) || (oldfd >= __OPEN_MAX) ||
      (newfd < 0) || (newfd >= __OPEN_MAX)) {
      return EBADF;
    }

  // Check oldfd is a valid file handle in filetable
  struct filetable *ft = curthread->t_filetable;
  spinlock_acquire(&ft->filetable_lock);
  if (ft->ft_entries[oldfd] == NULL) {
    return EBADF;
  }

  // Do nothing if old fd is the same as newfd
  if(oldfd == newfd) {
    *retval = newfd;
    spinlock_release(&ft->filetable_lock);
    return 0;
  }

  // return ENFILE error code if the process's file table was full
  if(ft->ft_entries[oldfd]->count > __OPEN_MAX) {
    spinlock_release(&ft->filetable_lock);
    return ENFILE;
  }

  // If newfd names an already-open file, that file is closed. 
  if (ft->ft_entries[newfd] != NULL) {
    file_close(newfd);
  }

  ft->ft_entries[newfd] = ft->ft_entries[oldfd];
  ft->ft_entries[newfd]->count += 1;
  *retval = newfd;
  spinlock_release(&ft->filetable_lock);

  return 0;

}

/*
 * sys_read
 * calls VOP_READ.
 * 
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
  // almost same as wrie.
  // except where the reading/writing is done
  struct uio user_uio;
  struct iovec user_iov;
  int result;
  int offset = 0;

  // checks if fd is a valid file descriptor.
  struct filetable *ft = curthread->t_filetable;
  spinlock_acquire(&ft->filetable_lock);

  // if fd is not a valid file descriptor,
  // or was not opened for reading, return EBADF
  if ((fd < 0) || (fd >= __OPEN_MAX) || (ft->ft_entries[fd] == NULL) ||
      (ft->ft_entries[fd]->file_vnode == NULL)) {
    spinlock_release(&ft->filetable_lock);
    return EBADF;
  }

  int x = ft->ft_entries[fd]->flags & O_ACCMODE;
  // if it was not opened for reading.
  if ((x != O_RDONLY) && (x != O_RDWR)) {
    spinlock_release(&ft->filetable_lock);
    return EBADF;
  }

  // set up a uio with the buffer, its size, and the current offset
  offset = ft->ft_entries[fd]->pos;
  mk_useruio(&user_iov, &user_uio, buf, size, offset, UIO_READ);

  // does the read

  spinlock_release(&ft->filetable_lock);
  result = VOP_READ(ft->ft_entries[fd]->file_vnode, &user_uio);
  if (result) {
    return result;
  }

  // The amount read is the size of the buffer originally, minus
  //how much is left in it.
  *retval = size - user_uio.uio_resid;

  spinlock_acquire(&ft->filetable_lock);
  // update file seeking position
  ft->ft_entries[fd]->pos += *retval;
  spinlock_release(&ft->filetable_lock);
  return 0;
}
// int
// sys_read(int fd, userptr_t buf, size_t size, int *retval)
// {
//   struct uio user_uio;
//   struct iovec user_iov;
//   int result;

//   // Check fd is a valid file handle
//   if ((fd < 0) || (fd >= __OPEN_MAX)){
//     return EBADF;
//   }
//   struct filetable *ft = curthread->t_filetable;
//   spinlock_acquire(&ft->filetable_lock);

//   // Check fd is a valid file handle in filetable and the vnode is not empty
//   if ((ft->ft_entries[fd] == NULL) || (ft->ft_entries[fd]->file_vnode == NULL)) {
//     spinlock_release(&ft->filetable_lock);
//     return EBADF;
//   }

//   // Check the file handle has the permission to read
//   int fl = ft->ft_entries[fd]->flags & O_ACCMODE;
//   if ((fl != O_RDONLY) && (fl != O_RDWR)) {
//     spinlock_release(&ft->filetable_lock);
//     return EBADF;
//   }

//   // Init the offset by the position of the file handle
//   int offset = ft->ft_entries[fd]->pos;

//   /* set up a uio with the buffer, its size, and the current offset */
//   mk_useruio(&user_iov, &user_uio, buf, size, offset, UIO_READ);

//   /* does the read */
//   result = VOP_READ(ft->ft_entries[fd]->file_vnode, &user_uio);
//   if (result) {
//     spinlock_release(&ft->filetable_lock);
//     return result;
//   }

//   /*
//    * The amount read is the size of the buffer originally, minus
//    * how much is left in it.
//    */
//   *retval = size - user_uio.uio_resid;

//   ft->ft_entries[fd]->pos += *retval;
//   spinlock_release(&ft->filetable_lock);

//   return 0;
// }

/*
 * sys_write
 * calls VOP_WRITE.
 *
 * A3: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */

int
sys_write(int fd, userptr_t buf, size_t len, int *retval) 
{
    struct uio user_uio;
    struct iovec user_iov;
    int result;
  // Check fd is a valid file handle
  if ((fd < 0) || (fd >= __OPEN_MAX)){
    return EBADF;
  }
  struct filetable *ft = curthread->t_filetable;
  spinlock_acquire(&ft->filetable_lock);

  // Check fd is a valid file handle in filetable and the vnode is not empty
  if ((ft->ft_entries[fd] == NULL) || (ft->ft_entries[fd]->file_vnode == NULL)) {
    spinlock_release(&ft->filetable_lock);
      return EBADF;
  }

  // Check the file handle has the permission to write
  int fl = ft->ft_entries[fd]->flags & O_ACCMODE;
  if ((fl != O_WRONLY) && (fl != O_RDWR)) {
    spinlock_release(&ft->filetable_lock);
    return EBADF;
  }

  // Init the offset by the position of the file handle
    int offset = ft->ft_entries[fd]->pos;

    /* set up a uio with the buffer, its size, and the current offset */
    mk_useruio(&user_iov, &user_uio, buf, len, offset, UIO_WRITE);

    /* does the write */
    result = VOP_WRITE(ft->ft_entries[fd]->file_vnode, &user_uio);
    if (result) {
      spinlock_release(&ft->filetable_lock);
        return result;
    }

    /*
     * the amount written is the size of the buffer originally,
     * minus how much is left in it.
     */
    *retval = len - user_uio.uio_resid;

    ft->ft_entries[fd]->pos += *retval;
    spinlock_release(&ft->filetable_lock);

    return 0;
}

/*
 * sys_lseek
 * 
 */
int
sys_lseek(int fd, off_t offset, int whence, off_t *retval)
{
        (void)fd;
        (void)offset;
        (void)whence;
        (void)retval;
  kprintf("dong sys_lseek");
	return EUNIMP;
}


/* really not "file" calls, per se, but might as well put it here */

/*
 * sys_chdir
 * 
 */
int
sys_chdir(userptr_t path)
{
    char *k_path;


    //check for kmalloc
    if((k_path = (char *)kmalloc(__PATH_MAX)) ==NULL){
    	return ENOMEM;
    }

    //copy into kernel
    int result;
    result = copyinstr(path,k_path,__PATH_MAX,NULL);

    //if fail
    if(result){
    	return result;
    }
    //else use vfs_chdir to do rest of the work

    result = vfs_chdir(k_path);

    kfree(k_path);
    return result;
    
}

/*
 * sys___getcwd
 * 
 */
int
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{
	//get the uio construct way from sys_read
  kprintf(" doing getcwd");
	struct uio user_uio;
	struct iovec user_iov;
	int result;
  //consturct uio
	mk_useruio(&user_iov, &user_uio, buf, buflen, 0, UIO_READ);
  //use vfs_getcwd to do the work
	result = vfs_getcwd(&user_uio);
  //return what vfs_getcwd returned
	*retval = result;
	return result;
}

/*
 * sys_fstat
 */
int
sys_fstat(int fd, userptr_t statptr)
{
  struct stat kstat;
  int result;
  struct filetable *file_table = curthread->t_filetable;
  spinlock_acquire(&file_table->filetable_lock);
  //kprintf(" doing fstat");
  //check for fd is valid or not
  if((fd > __OPEN_MAX) || (file_table->ft_entries[fd] == NULL) || (fd<0)){
    spinlock_release(&file_table->filetable_lock);
    return EBADF;
  }
  //use vop_stat to return the info of file
  result = VOP_STAT(file_table->ft_entries[fd]->file_vnode, &kstat);
  if(result){
    return result;
  }
  spinlock_release(&file_table->filetable_lock);
  //kprintf(" fstat done");
	return copyout(&kstat, statptr, sizeof(struct stat));
}

/*
 * sys_getdirentry
 */
int
sys_getdirentry(int fd, userptr_t buf, size_t buflen, int *retval)
{
  struct filetable *file_table = curthread->t_filetable;
  spinlock_acquire(&file_table->filetable_lock);
  //kprintf("doing get dir");
  //check the fd is valid or not like we do in file_close
  if((fd > __OPEN_MAX) || (curthread->t_filetable->ft_entries[fd] == NULL) || (fd<0)){
    return EBADF;
  }

  //check the flag
  int how;
  how = file_table->ft_entries[fd]->flags & O_ACCMODE;
  if(how == O_WRONLY){
    spinlock_release(&file_table->filetable_lock);
    return EBADF;
  }

  //construct uio
  struct uio user_uio;
  struct iovec user_iov;
  int offset = file_table->ft_entries[fd]->pos;
  mk_useruio(&user_iov, &user_uio, buf, buflen, offset, UIO_READ);

  int result;
  struct vnode *vn;
  vn = file_table->ft_entries[fd]->file_vnode;
  //read the file into uio
  result = VOP_GETDIRENTRY(vn,&user_uio);
  //if fail
  if(result){
    spinlock_release(&file_table->filetable_lock);
    return result;
  }

  //on success upadate the offset
  file_table->ft_entries[fd]->pos = user_uio.uio_offset;
  //set retval to be the amount we read
  *retval = buflen - user_uio.uio_resid;
  spinlock_release(&file_table->filetable_lock);
  //kprintf("get dir done");
	return 0;
}

/* END A3 SETUP */




