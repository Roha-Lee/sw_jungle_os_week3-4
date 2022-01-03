#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/flags.h"
#include "threads/init.h" // power_off 부르기 위해  
#include "filesys/filesys.h" // create, remove에서 함수 사용하기 위해 
#include "userprog/gdt.h"
#include "intrinsic.h"
void check_address(void *addr);
struct file *fd_to_struct_filep(int fd);

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void halt (void);
void exit (int);
bool create (const char *file , unsigned initial_size);
bool remove (const char *file);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */


// [Project 2-2]
void check_address(void *addr){
	struct thread *current = thread_current ();
	
	if (addr == NULL || is_kernel_vaddr(addr) || pml4e_walk(current->pml4, addr, false) == NULL){
		exit(-1);
	}	
}


void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("system call! %d\n", f->R.rax);
	int syscall_no = f->R.rax;
	// a1 = f->R.rdi
	// a2 = f->R.rsi
	// a3 = f->R.rdx
	// a4 = f->R.r10
	// a5 = f->R.r8
	// a6 = f->R.r9
	// return = f->R.rax
	switch(syscall_no){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_CREATE:
			check_address(f->R.rdi);
			create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			check_address(f->R.rdi);
			remove(f->R.rdi);
			break;
		
		// case SYS_WRITE:
		// 	check_address(f->R.rsi);
		// 	f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		// 	break;
		default:
			break;
	}
	
	
}


void
halt (void) {
	power_off();
}


void
exit (int status) {
	int status = 0; 
	printf("%s: exit(%d)\n", thread_name(), status);
	thread_exit ();
}


bool create (const char *file , unsigned initial_size){
	return filesys_create(file, initial_size);
}

bool remove (const char *file){
	return filesys_remove(file);
}
// struct file *fd_to_struct_filep(int fd){
// }

// int 
// write (int fd, const void *buffer, unsigned size){
// 	// struct file *f; // some function that maps df to struct file *file;
// 	// file_write_at(f);
// }


