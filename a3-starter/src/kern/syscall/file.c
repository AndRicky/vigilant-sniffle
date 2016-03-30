/* BEGIN A3 SETUP */
/*
 * File handles and file tables.
 * New for ASST3
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <file.h>
#include <syscall.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
#include <spinlock.h>
#include <kern/fcntl.h>


/*** openfile functions ***/

/*
 * file_open
 * opens a file, places it in the filetable, sets RETFD to the file
 * descriptor. the pointer arguments must be kernel pointers.
 * NOTE -- the passed in filename must be a mutable string.
 * 
 * A3: As per the OS/161 man page for open(), you do not need 
 * to do anything with the "mode" argument.
 */

int
file_open(char *filename, int flags, int mode, int *retfd)
{
	kprintf("doing openfile");
	int how;
	struct filetable_entry *f_entry;
	(void) mode;
	//check the flag like the do in vfs_open
	kprintf("checking flag");
	how = flags & O_ACCMODE;
	//if the flag is invalid return error code
	if((how != O_RDONLY) && (how != O_WRONLY) && (how  != O_RDWR)) {
		kprintf("wrong flag in file_open\n");
		return EINVAL;
	}

	int i;
	kprintf("struct the file table");
	struct filetable *file_table = curthread->t_filetable;

	//acquire the lock
	spinlock_acquire(&file_table->filetable_lock);
	//find a empty space in the file table
	kprintf("finding empty entry\n");
	for (i = 0;i <__OPEN_MAX;i ++){
		if(file_table->ft_entries[i] == NULL){
			break;
		}
	}

	//if the file_table is full
	if(i == __OPEN_MAX){
		spinlock_release(&file_table->filetable_lock);
		return EMFILE;
	}

	kprintf("empty entry found");
	//find an empty space
	//malloc the size for entry
	f_entry = (struct filetable_entry *)kmalloc(sizeof(struct filetable_entry));
	//if malloc fail
	if(!f_entry){
		spinlock_release(&file_table->filetable_lock);
		return ENOMEM;
	}
	//construct the node
	struct vnode *vn = NULL;
	int vfs_result;
	vfs_result = vfs_open(filename, flags, 0, &vn);

	//if open fail

	if (vfs_result){
		kfree(f_entry);
		spinlock_release(&file_table->filetable_lock);
		return vfs_result;
	}
	//put the entry to filetable
	file_table->ft_entries[i] = f_entry;
	//fill the entry
	file_table->ft_entries[i]->file_vnode = vn;
	file_table->ft_entries[i]->flags = flags;
	file_table->ft_entries[i]->count = 1;
	file_table->ft_entries[i]->pos = 0;
	//put the entry to filetable
	//set the retfd
	*retfd = i;
	spinlock_release(&file_table->filetable_lock);

	return 0;
}

/* 
 * file_close
 * Called when a process closes a file descriptor.  Think about how you plan
 * to handle fork, and what (if anything) is shared between parent/child after
 * fork.  Your design decisions will affect what you should do for close.
 */
int
file_close(int fd)
{
	kprintf("file close\n");
	//check the fd is valid or not
	if((fd >= __OPEN_MAX) || (curthread->t_filetable->ft_entries[fd] == NULL) || (fd<0)){
		return EBADF;
	}
	struct filetable *file_table = curthread->t_filetable;

	spinlock_acquire(&file_table->filetable_lock);
	//decrease the count by 1
	curthread->t_filetable->ft_entries[fd]->count --;
	//if the count is 0, delete the entry, else just leave it there
	if(curthread->t_filetable->ft_entries[fd]->count == 0){
		vfs_close(curthread->t_filetable->ft_entries[fd]->file_vnode);
		kfree(curthread->t_filetable->ft_entries[fd]);
		
	}
	curthread->t_filetable->ft_entries[fd] = NULL;
	spinlock_release(&file_table->filetable_lock);

	return 0;
}

/*** filetable functions ***/

/* 
 * filetable_init
 * pretty straightforward -- allocate the space, set up 
 * first 3 file descriptors for stdin, stdout and stderr,
 * and initialize all other entries to NULL.
 * 
 * Should set curthread->t_filetable to point to the
 * newly-initialized filetable.
 * 
 * Should return non-zero error code on failure.  Currently
 * does nothing but returns success so that loading a user
 * program will succeed even if you haven't written the
 * filetable initialization yet.
 */
int
filetable_init(void)
{
    // all the variables
    kprintf("filetable init\n");
    struct filetable *ft = (struct filetable *)kmalloc(sizeof(struct filetable));
    int result;
    char path[5];
    strcpy(path, "con:");
    int fd;

    // forgot to set the current thread filetable
    curthread->t_filetable = ft;
    // setup the first 3 table entries
    // initalize all entries to null
    for(fd = 0; fd < __OPEN_MAX; fd++) {

        ft->ft_entries[fd] = NULL;
    }
    struct vnode *file = NULL;
    if((result = vfs_open(path, O_RDWR, 0, &file))) {
        // file open failed
        // clean up
        kfree(ft);
        return result;
    }
    
    // file open complete set up the fds
    ft->ft_entries[0] = (struct filetable_entry *)kmalloc(sizeof(struct filetable_entry));
    ft->ft_entries[0]->file_vnode = file;
    ft->ft_entries[0]->flags = O_RDWR;
    // since we can't open the same file twice we need to create links to it
    ft->ft_entries[0]->count = 3;
    ft->ft_entries[0]->pos = 0;

    // points to the first fd
    ft->ft_entries[1] = ft->ft_entries[0];
    ft->ft_entries[2] = ft->ft_entries[0];
    spinlock_init(&ft->filetable_lock);
	return 0;
}
// int
// filetable_init(void)
// {
// 	// Allocate the space of filetable and the entry of filetable
// 	struct filetable *ft = (struct filetable *) kmalloc(sizeof(struct  filetable));
// 	ft->ft_entries[0] = (struct filetable_entry *)kmalloc(sizeof(struct filetable_entry));
	
// 	// Get a vnode for the * console device
// 	struct vnode *cons_vnode=NULL; 
// 	int result;
// 	char path[5];
// 	strcpy(path, "con:");
//   	result = vfs_open(path, O_RDWR, 0, &cons_vnode);
//   	// Set up the first 3 file descriptors
//   	ft->ft_entries[0]->file_vnode = cons_vnode;
//   	ft->ft_entries[0]->flags = O_RDWR;
//   	ft->ft_entries[0]->pos = 0;
//   	ft->ft_entries[0]->count = 3;
//   	ft->ft_entries[1] = ft->ft_entries[0];
//   	ft->ft_entries[2] = ft->ft_entries[0];

//   	// Init all other entries to NULL
//   	int i = 3;
//   	while (i < __OPEN_MAX) {
//   		ft->ft_entries[i] = NULL;
//   		i++;
//   	}
//   	// Init the filetable spinlock
//   	spinlock_init(&ft->filetable_lock);

//   	//Set curthread->t_filetable to point to the newly-initialized filetable
//   	curthread->t_filetable = ft;
// 	return 0;
// }	

/*
 * filetable_destroy
 * closes the files in the file table, frees the table.
 * This should be called as part of cleaning up a process (after kill
 * or exit).
 */
void
filetable_destroy(struct filetable *ft)
{
	int i = 0;
	while(i < __OPEN_MAX) {
		struct filetable_entry *ft_e = ft->ft_entries[i];
		if (ft_e != NULL) {
			file_close(i);
		}
	}
	spinlock_cleanup(&ft->filetable_lock);
	kfree(ft);
}	


/* 
 * You should add additional filetable utility functions here as needed
 * to support the system calls.  For example, given a file descriptor
 * you will want some sort of lookup function that will check if the fd is 
 * valid and return the associated vnode (and possibly other information like
 * the current file position) associated with that open file.
 */


/* END A3 SETUP */
