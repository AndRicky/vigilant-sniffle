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
	(void)filename;
	(void)flags;
	(void)retfd;
	(void)mode;


	return EUNIMP;
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
        (void)fd;

	return EUNIMP;
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
	// Allocate the space of filetable and the entry of filetable
	struct filetable *ft = (struct filetable *) kmalloc(sizeof(struct  filetable));
	ft->ft_entries[0] = (struct ft_entry)kmalloc(sizeof(struct ft_entry));
	
	// Get a vnode for the * console device
	struct vnode *cons_vnode=NULL; 
	int = result;
	char path[5];
	strcpy(path, "con:");
  	result = vfs_open(path, O_RDWR, 0, &cons_vnode);
  	// Set up the first 3 file descriptors
  	ft->ft_entries[0]->file_vnode = cons_vnode;
  	ft->ft_entries[0]->flags = O_RDWR;
  	ft->ft_entries[0]->pos = 0;
  	ft->ft_entries[0]->count = 3;
  	ft->ft_entries[1] = ft->ft_entries[0];
  	ft->ft_entries[2] = ft->ft_entries[0];

  	// Init all other entries to NULL
  	int i = 3;
  	while (i < __OPEN_MAX) {
  		ft->ft_entries[i] = NULL;
  		i++;
  	}
  	// Init the filetable spinlock
  	spinlock_init(&ft->ft_spinlock);

  	//Set curthread->t_filetable to point to the newly-initialized filetable
  	curthread->t_filetable = ft;
	return 0;
}	

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
		struct filetable_entry *ft_e = ft_ft_entries[i];
		if (ft_e != NULL) {
			file_close(i);
		}
	}
	spinlock_cleanup(&ft->ft_spinlock);
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
