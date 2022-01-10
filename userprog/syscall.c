#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/flags.h"
#include "threads/synch.h"
#include "threads/init.h" 
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h" 
#include "userprog/gdt.h"
#include "intrinsic.h"


void check_address(void *addr);
struct file *fd_to_struct_filep(int fd);
int add_file_to_fd_table(struct file *file);
void remove_file_from_fd_table(int fd);

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void halt (void);
void exit (int);
void close (int fd);
bool create (const char *file , unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
int filesize (int fd);
unsigned tell (int fd);
void seek (int fd, unsigned position);
int fork (const char *thread_name, struct intr_frame *tf);
int wait (tid_t child_tid);
int exec (const char *cmd_line);
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
void 
check_address(void *addr){
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
	
	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call! %d\n", f->R.rax);
	int syscall_no = f->R.rax;
	struct thread * current = thread_current();

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
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			check_address(f->R.rdi);
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_WRITE:
			check_address(f->R.rsi);
			check_address(f->R.rsi + f->R.rdx - 1);
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_OPEN:
			check_address(f->R.rdi);
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize (f->R.rdi);
			break;
		case SYS_READ:
			check_address(f->R.rsi);
			check_address(f->R.rsi + f->R.rdx - 1);
			f->R.rax = read (f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek (f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell (f->R.rdi);
			break;
		case SYS_WAIT:
			f->R.rax = wait (f->R.rdi);
			break;
		case SYS_EXEC:
			check_address(f->R.rdi);
			f->R.rax = exec(f->R.rdi);
			if(f->R.rax == -1){
				exit(-1);
			}
			break;
		case SYS_FORK:
			check_address(f->R.rdi);
			f->R.rax = fork(f->R.rdi, f);
			break;
		default:
			exit(-1);
			break;
	}	
}

struct file *
fd_to_struct_filep(int fd) {
	if (fd < 0 || fd >= MAX_FD_NUM){
		return NULL;
	}
	struct thread * current = thread_current();
	return current->fd_table[fd];
}

int 
add_file_to_fd_table(struct file *file){
	int fd = 2;
	struct thread * current = thread_current();
	while(current->fd_table[fd] != NULL && fd < MAX_FD_NUM){
		fd++;
	}
	if(fd >= MAX_FD_NUM){
		return -1;
	}
	current->fd_table[fd] = file;
	return fd;
}

void 
remove_file_from_fd_table(int fd){
	struct thread * current = thread_current();
	if (fd < 0 || fd >= MAX_FD_NUM){
		return;
	}	
	current->fd_table[fd] = NULL;
}

void
halt (void) {
	power_off();
}


void
exit (int status) {
	struct thread *current = thread_current();
	current->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), status);
	thread_exit ();
}


bool 
create (const char *file , unsigned initial_size){
	lock_acquire(&filesys_lock);
	bool return_value = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return return_value;
}

bool 
remove (const char *file){
	lock_acquire(&filesys_lock);
	bool return_value = filesys_remove(file);
	lock_release(&filesys_lock);
	return return_value;
}

int 
read (int fd, void *buffer, unsigned size) {
	off_t read_byte;
	uint8_t *read_buffer = buffer;
	if(fd == STDIN_FILENO) {
		char key;
		for (read_byte = 0; read_byte < size; read_byte++){
			key = input_getc();
			*read_buffer++ = key;
			if(key == '\0'){
				break;
			}
		}
	}
	else if(fd == STDOUT_FILENO){
		return -1;
	}
	else {
		struct file * read_file = fd_to_struct_filep(fd);
		if(read_file == NULL){
			return -1;
		}
		lock_acquire(&filesys_lock);
		read_byte = file_read(read_file, buffer, size);
		lock_release(&filesys_lock);
	}
	return read_byte;
}

int 
write (int fd, const void *buffer, unsigned size){
	if (fd == STDIN_FILENO) {
		return 0;
	}
	else if(fd == STDOUT_FILENO){
		putbuf(buffer, size);
		return size;
	}
	else {
		
		struct file * write_file = fd_to_struct_filep(fd);
		if(write_file == NULL){
			return 0;
		}
		lock_acquire(&filesys_lock);
		off_t write_byte = file_write(write_file, buffer, size);
		lock_release(&filesys_lock);
		return write_byte;
	}
}

int 
open (const char *file){
	lock_acquire(&filesys_lock);
	struct file * open_file = filesys_open(file);
	lock_release(&filesys_lock);
	if(open_file == NULL){
		return -1;
	}
	int fd = add_file_to_fd_table(open_file);
	if (fd == -1){
		lock_acquire(&filesys_lock);
		file_close(open_file);
		lock_release(&filesys_lock);
	}
	return fd;
}

void 
close (int fd){
	struct file *close_file = fd_to_struct_filep(fd);
	if(close_file == NULL){
		return;
	}
	lock_acquire(&filesys_lock);
	file_close(close_file);
	lock_release(&filesys_lock);
	remove_file_from_fd_table(fd);
}

int 
filesize (int fd){
	struct file *f = fd_to_struct_filep(fd);
	if(f == NULL){
		return -1;
	}
	lock_acquire(&filesys_lock);
	off_t return_val = file_length(f);
	lock_release(&filesys_lock);
	return (int) return_val;
}

unsigned 
tell (int fd){
	struct file *f = fd_to_struct_filep(fd);
	if(f == NULL){
		return 0;
	}
	lock_acquire(&filesys_lock);
	off_t return_val = file_tell(f);
	lock_release(&filesys_lock);
	return (unsigned) return_val;
}

void 
seek (int fd, unsigned position){
	struct file *f = fd_to_struct_filep(fd);
	if(f == NULL){
		return 0;
	}
	lock_acquire(&filesys_lock);
	file_seek(f, (off_t) position);
	lock_release(&filesys_lock);
}

int
fork (const char *thread_name, struct intr_frame *tf){
	return process_fork(thread_name, tf);
}

int 
wait (tid_t child_tid){
	return process_wait(child_tid);
}

int exec (const char *cmd_line){
	int size = strlen(cmd_line) + 1;
	char *fn_copy = palloc_get_page (PAL_ZERO);
	if (fn_copy == NULL){
		return -1;
	}
	strlcpy (fn_copy, cmd_line, PGSIZE);
	if (process_exec (fn_copy) < 0){
		return -1;
	}
}