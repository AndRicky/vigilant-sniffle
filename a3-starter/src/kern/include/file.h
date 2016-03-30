/* BEGIN A3 SETUP */
/*
 * Declarations for file handle and file table management.
 * New for A3.
 */

#ifndef _FILE_H_
#define _FILE_H_

#include <kern/limits.h>
#include <spinlock.h>

struct vnode;

/*
 * filetable struct
 * just an array, nice and simple.  
 * It is up to you to design what goes into the array.  The current
 * array of ints is just intended to make the compiler happy.
 */
struct filetable_entry {
	struct vnode *file_vnode;
	int pos; //position in file
	int flags; //open flags
	int count; //number of file descriptors to this entry
};
/*
* filetable struct
* just an array, nice and simple.
* It is up to you to design what goes into the array. The current
* array of ints is just intended to make the compiler happy.
*/
struct filetable {
	struct filetable_entry *ft_entries[__OPEN_MAX];
	struct spinlock filetable_lock;
// int changeme[__OPEN_MAX]; /* dummy type */
};

/* these all have an implicit arg of the curthread's filetable */
int filetable_init(void);
void filetable_destroy(struct filetable *ft);

/* opens a file (must be kernel pointers in the args) */
int file_open(char *filename, int flags, int mode, int *retfd);

/* closes a file */
int file_close(int fd);

/* A3: You should add additional functions that operate on
 * the filetable to help implement some of the filetable-related
 * system calls.
 */

#endif /* _FILE_H_ */

/* END A3 SETUP */
