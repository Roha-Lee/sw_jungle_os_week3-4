#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/off_t.h"
#include <stdbool.h>

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

// project 3
bool install_page (void *upage, void *kpage, bool writable);
bool lazy_load_segment (struct page *page, void *aux);
// bool setup_stack(struct intr_frame *if_);
// project 3
struct container{
    struct file *file;
    off_t offset;
    size_t page_read_bytes;
};

#endif /* userprog/process.h */
