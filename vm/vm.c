/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"


/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

//typedef void hash_action_func (struct hash_elem *e, void *aux);
void
destroy_pages(struct hash_elem *e, void *aux){
	// destructor (hash_elem, h->aux); 와 같이 불림 
	struct page * p = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(p);
}

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page * newpage = malloc(sizeof (struct page));
		switch(VM_TYPE(type)){
			case VM_ANON:
				uninit_new(newpage, upage, init, type, aux, anon_initializer);
				break;
			default:
				break;
		}
		newpage->uninit.aux = aux;
		newpage->writable = writable;
		
		/* TODO: Insert the page into the spt. */
		bool success = spt_insert_page(spt, newpage);
		if (!success) {
			free(newpage);
		}
		return success;
	}
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */	
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	
	/* TODO: Fill this function. */
	struct page empty_page;
	empty_page.va = pg_round_down(va);
	lock_acquire(&spt->hash_lock);
	struct hash_elem *e = hash_find(&spt->pages, &empty_page.hash_elem);
	lock_release(&spt->hash_lock);
	if (e == NULL){
		return e;	
	}
	page = hash_entry(e, struct page, hash_elem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	lock_acquire(&spt->hash_lock);
	struct hash_elem *e = hash_insert(&spt->pages, &page->hash_elem);
	lock_release(&spt->hash_lock);
	if (e == NULL){
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	
	void * newpage = palloc_get_page(PAL_USER); 
	if(newpage == NULL) {
		// eviction
		PANIC("todo");
	}
	else {
		frame = malloc(sizeof (struct frame));
		frame->kva = newpage;
		frame->page = NULL;
	}
	
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (user && is_kernel_vaddr(addr)) {
		return false;
	}
	page = spt_find_page(spt, addr);
	if (page == NULL) {
		return false;
	}
	if (write && !page->writable) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread *current = thread_current();
	page = spt_find_page(&current->spt, va);
	if(page == NULL){
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread * current = thread_current();
	if(!pml4_set_page(current->pml4, page->va, frame->kva, page->writable)) {
		return false;
	}
	
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	lock_init(&spt->hash_lock);
	lock_acquire(&spt->hash_lock);
	hash_init(&spt->pages, page_hash, page_less, NULL);
	lock_release(&spt->hash_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;
	struct hash * src_hash = &src->pages;
	hash_first (&i, src_hash);
	while (hash_next (&i)) {
    	struct page *p = hash_entry (hash_cur (&i), struct page, hash_elem);
		enum vm_type type = page_get_type(p);
		void *upage = p->va;
		bool writable = p->writable;
		bool success = false;
		vm_initializer *init = p->uninit.init;
		void *aux = p->uninit.aux;
		if(p->operations->type == VM_UNINIT){
			if (!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
				return false;
		}
		else if(type == VM_ANON) {
			if (!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
				return false;
			if (!vm_claim_page(upage))
				return false;
			struct page* newpage = spt_find_page(dst, upage);
			memcpy(newpage->frame->kva, p->frame->kva, PGSIZE);
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	lock_acquire(&spt->hash_lock);
	hash_destroy(&spt->pages, destroy_pages);
	lock_release(&spt->hash_lock);
}

