#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/flags.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "threads/flags.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <list.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
//project 3
#include "vm/vm.h"
const int STDIN = 1;
const int STDOUT = 2;
void syscall_entry (void);
void syscall_handler (struct intr_frame *);
// void check_address(void *addr);
struct page * check_address(void *addr); // project 3
// void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write); // project 3
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);
static struct file *find_file_by_fd(int fd);


void halt(void);
void exit(int status);
tid_t fork(const char *thread_name, struct intr_frame *f);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
// project 3
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);

void syscall_handler(struct intr_frame *);

void check_address(uaddr);
void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

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

void
syscall_init (void) {
	lock_init (&filesys_lock);
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	lock_init(&file_rw_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.

	#ifdef VM
		thread_current()->rsp_stack = f->rsp; //syscall ????????? user process??? user stack pointer
	#endif
	switch (f->R.rax)
	{
		case SYS_HALT:
		{
			halt();
			NOT_REACHED();
			break;
		}

		case SYS_EXIT:
		{
			exit(f->R.rdi);
			NOT_REACHED();
			break;
		}

		case SYS_FORK:
		{
			f->R.rax = fork(f->R.rdi, f);
			break;
		}
			
		case SYS_EXEC:
		{
			if (exec(f->R.rdi) == -1) {
				exit(-1);
			}
			break;
		}
			
		case SYS_WAIT:
		{
			f->R.rax = process_wait(f->R.rdi);
			break;
		}
			
		case SYS_CREATE:
		{
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		}
			
		case SYS_REMOVE:
		{
			f->R.rax = remove(f->R.rdi);
			break;
		}
			
		case SYS_OPEN:
		{
			f->R.rax = open(f->R.rdi);
			break;
		}
			
		case SYS_FILESIZE:
		{
			f->R.rax = filesize(f->R.rdi);
			break;
		}
			
		case SYS_READ:
		{
			// check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 1);
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		}
			
		case SYS_WRITE:
		{
			// check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 0);
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		}

		case SYS_SEEK:
		{
			seek(f->R.rdi, f->R.rsi);
			break;
		}
			
		case SYS_TELL:
		{
			f->R.rax = tell(f->R.rdi);
			break;
		}
			
		case SYS_CLOSE:
		{
			close(f->R.rdi);
			break;
		}
		// project 3
		case SYS_MMAP:
			f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
			break;
		case SYS_MUNMAP:
			munmap(f->R.rdi);
			break;
		default:
			exit(-1);
			break;
			
	}
	// printf ("system call!\n");
	// thread_exit ();
}

/*-------------project 2 syscall --------------------*/

void exit(int status)
{
	struct thread *cur = thread_current();
	cur->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), status);
	thread_exit();
}

void halt(void)
{
	power_off();
}

bool create(const char *filename, unsigned initial_size) 
{
	// project 3
	if (!filename){
		exit(-1);
	}
	bool return_code;
	check_address(filename);

	lock_acquire(&filesys_lock); 
	return_code = filesys_create(filename, initial_size);
	lock_release (&filesys_lock);
	return return_code;

}

bool remove(const char *filename)
{
	bool return_code;
	check_address(filename);
	lock_acquire(&filesys_lock);
	return_code = filesys_remove(filename);
	lock_release(&filesys_lock);
	return return_code;
}

// ?????? ???????????????. 
// syscall ?????? ??????????????? exec(f->R.rdi) . ?????? cmdline????????? ??? ??? ??????? 
// ????????? ???????????? int ??? ?????? pid_t ???. ?????? ?????????. 
int exec(char *cmdline)
{
	check_address(cmdline);

	// process_exec ??? process_cleanup????????? f->R.rdi ??? ?????????????????? 
	// process cleanup??? ????????? f (intr_frame) ??? ????????? ???????
	// ?????? ???????????? ?????????????????? context switching??? ?????? ????????????. 
	int file_size = strlen(cmdline) + 1;
	char *cmd_copy = palloc_get_page(PAL_ZERO);
	if (cmd_copy == NULL) {
		exit(-1);
	}
	strlcpy(cmd_copy, cmdline, file_size);

	if (process_exec(cmd_copy) == -1) {
		return -1;
	}
	NOT_REACHED();
	return 0;
}

// ??????
tid_t fork(const char *thread_name, struct intr_frame *f) {
	check_address(thread_name);
	return process_fork(thread_name, f);
} 

// ????????? ???????????? fd??? ?????? ???????????? ??????. buffer??? ??? ?????? ????????? ??????.
int write(int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	check_valid_buffer(buffer, size, true);
	int write_result;
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj == NULL) {
		return -1;
	}
	struct thread *cur = thread_current();
	if (fd == 1) {
		putbuf(buffer, size);  // ??????????????? ???????????? ?????? putbuf()
		write_result = size;
	} else if (fd == 0) {
		write_result = -1;
	} else {
		lock_acquire(&filesys_lock);
		write_result = file_write(fileobj, buffer, size);
		lock_release(&filesys_lock);
	}
	return write_result;
}

int read(int fd, void *buffer, unsigned size) {
	check_address(buffer);
	check_valid_buffer(buffer, size, false);
	int read_result;
	struct thread *cur = thread_current();
	struct file *file_fd = find_file_by_fd(fd);
	if (file_fd == NULL) {
		return -1;
	}
	if (fd == 0) {
		int i;
		unsigned char *buf = buffer;
		for (i = 0; i < size; i++) {
			char c = input_getc();
			*buf++ = c;
			if (c == '\0') {
				break;
			}
		}
		read_result = i;
	} else if (fd == 1) {
		read_result = -1;
	} else {
		lock_acquire(&filesys_lock);
		read_result = file_read(file_fd, buffer, size);
		lock_release(&filesys_lock);
	}
	return read_result;
}
// fd ????????? ????????? ?????? ????????? ??????
int filesize(int fd) {
	struct file *open_file = find_file_by_fd(fd);
	if (open_file == NULL) {
		return -1;
	}
	return file_length(open_file);
}

// fd ??? ??????, ????????? -1 ??????. ?????? ?????? ??????
int open(const char *file) {
	check_address(file);

	// project 3
	if(file == NULL){
		return -1;
	}

	struct file *open_file = filesys_open(file);
	lock_release(&filesys_lock);
	if (open_file == NULL) {
		return -1;
	}
	int fd = add_file_to_fdt(open_file);

	//fd talbe ??? ?????? ????????? 
	if (fd == -1) {
		file_close(open_file);
	}
	return fd;
}

// ?????? ?????? (offset)?????? ???????????? ??????
// read ??? ????????? ?????? ???????????? seek??? ??? ????????? ????????? ??????????????? ????????? ?????????.
void seek(int fd, unsigned position) {
	if (fd <= 1) {   // 0: ????????????, 1: ?????? ??????
		return;
	}
	struct file *seek_file = find_file_by_fd(fd);
	seek_file->pos = position;
}
// tell ????????? ?????? ???????????? ????????? seek ????????? ???????????? ????????????. 
unsigned tell(int fd) {
	if (fd <= 1) {
		return;
	} 
	struct file *tell_file = find_file_by_fd(fd);
	return file_tell(tell_file);
}

void close(int fd) {
	struct file *fileobj = find_file_by_fd(fd);
	if (fileobj == NULL) {
		return;
	}
	if (fileobj <= 2)     // stdin ?????? stdout ??? ????????? ?????????.
		return;
	remove_file_from_fdt(fd);
}



/*-------------project 2 syscall --------------------*/

/*-------------project 3 syscall --------------------*/
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset){
	if (offset % PGSIZE != 0){
		return NULL;
	}
	if(pg_round_down(addr) != addr || is_kernel_vaddr(addr) || addr == NULL || (long long)length <=0)
		return NULL;
	// console input, output??? mapping x
	if(fd == 0 || fd == 1)
		exit(-1);
	if(spt_find_page(&thread_current()->spt, addr))
		return NULL;
	
	struct file *target = process_get_file(fd);
	if(target == NULL)
		return NULL;
	
	void *ret = do_mmap(addr, length, writable, target, offset);

	return ret;
}

void munmap(void *addr){
	do_munmap(addr);
}
/*-------------project 3 syscall --------------------*/


/*------------- project 2 helper function -------------- */
// void check_address(void *addr)
// {
// 	struct thread *cur = thread_current();
// 	if (addr == NULL || !is_user_vaddr(addr) || pml4_get_page(cur->pml4, addr) == NULL)
// 	{
// 		exit(-1);
// 	}
// }

// project 3
// ???????????? rsp????????? check_address??? ??????????????? ???????????????
struct page * check_address(void *addr){
	if(is_kernel_vaddr(addr)){
		exit(-1);
	}
	return spt_find_page(&thread_current()->spt,addr);
}

void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write){
	for(int i=0; i<size; i++){
		struct page* page = check_address(buffer + i);
		if(page == NULL)
			exit(-1);
		if(to_write == true && page->writable == false)
			exit(-1);
	}
}

// fd ??? ?????? ?????? ??????
static struct file *find_file_by_fd(int fd) {
	struct thread *cur = thread_current();
	if (fd < 0 || fd >= FDCOUNT_LIMIT) {
		return NULL;
	}
	return cur->fd_table[fd];
}

// ?????? ??????????????? fd ???????????? ?????? ??????.
int add_file_to_fdt(struct file *file) 
{
	struct thread *cur = thread_current();
	struct file **fdt = cur->fd_table;
	// fd??? ????????? ?????? ????????? ?????? ??????, fd table??? ????????? ????????? ???????????????
	while (cur->fd_idx < FDCOUNT_LIMIT && fdt[cur->fd_idx]) {
		cur->fd_idx++;
	}	
	// error. fd table full
	if (cur->fd_idx >= FDCOUNT_LIMIT) {
		return -1;
	}
	fdt[cur->fd_idx] = file;
	return cur->fd_idx;
}

// fd ???????????? ?????? ????????? ??????
void remove_file_from_fdt(int fd)
{
	struct thread *cur = thread_current();

	// if invalid fd
	if (fd < 0 || fd >= FDCOUNT_LIMIT){
		return;
	}
	cur->fd_table[fd] = NULL;
}

/* -----------------project 2 helper functions -----------*/