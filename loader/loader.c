/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "exec_parser.h"

static so_exec_t *exec;
static int fd;
static struct sigaction old_handler;

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	/* TODO - actual loader implementation */
	so_seg_t *segm;
	int found = 0;

	/* First, we need to verify if the signal received is not SIGSEGV */
	if (signum != SIGSEGV) {
		old_handler.sa_sigaction(signum, info, context);
		return;
	}

	/* We need the page size in order to find the index of the page */
	int pagesz = getpagesize();

	/* Also, we need the adress where the page fault happens
	 * (generate a page fault) */
	uintptr_t page_fault_addr = (int)info->si_addr;

	/* We go through the list of segments trying to find the segment
	 * where it crashed (if it crashes) */
	int i;

	for (i = 0; i < exec->segments_no; i++)
	/* If the if statement is true, then the crash happens here */
		if (page_fault_addr >= exec->segments[i].vaddr && page_fault_addr <
			(exec->segments[i].vaddr + exec->segments[i].mem_size)) {
			found = 1;
			break;
		}

	/* If we didn't find it in the list of segments, then it is seg fault */
	if (found == 0) {
		old_handler.sa_sigaction(signum, info, context);
		return;
	}

	segm = &exec->segments[i];
	/* Store the index of the page we need to map */
	int page_index = (page_fault_addr - segm->vaddr) / pagesz;

	/* Verify if there is allocated space for data field */
	if (segm->data == NULL)
		segm->data = calloc(segm->mem_size, sizeof(int));

	/* Verify if the page is already mapped.*/
	if (((int *)segm->data)[page_index] == 1) {
		old_handler.sa_sigaction(signum, info, context);
		return;
	}

	/* We mark the found page as mapped.*/
	((int *)exec->segments[i].data)[page_index] = 1;

	int fault_page = pagesz * page_index;

	/* We map it all */
	void *mmap_addr = mmap((void *) segm->vaddr + fault_page,
							pagesz, segm->perm, MAP_PRIVATE | MAP_FIXED, fd,
							segm->offset + fault_page);

	/* Verify each case where the page fault can happen in the segment and
	 * put 0 in those specific portions */

	/* First, we verify if the beginning is in the file size, and the page
	 * ends after it and it's still in the memory size. */
	if (fault_page <= segm->file_size &&
		fault_page + pagesz >= segm->file_size &&
		fault_page + pagesz <= segm->mem_size)
		memset((void *)segm->vaddr + segm->file_size, 0, 
			   (int)(fault_page + pagesz - segm->file_size));
	/* Then, it is verified if the whole page is contained in the memory size*/
	else if (fault_page >= segm->file_size &&
			fault_page + pagesz <= segm->mem_size)
		memset(mmap_addr, 0, (int)pagesz);
	/* After that, it is verified if the end of the page goes beyond the memory
	 * size, in that case, we ignore what goes after the memory size.*/
	else if (fault_page >= segm->file_size &&
			 fault_page + pagesz >= segm->mem_size)
		memset(mmap_addr, 0, (int)(segm->mem_size - pagesz * page_index));
	/* In the end, it is verified if the beginning of the page is in the file
	 * size, and the end of it goes beyond the memory size. In that case, 0s
	 * will be put between the file size and memory size portions.*/
	else if (fault_page <= segm->file_size &&
			 fault_page + pagesz >= segm->mem_size)
		memset((void *)segm->vaddr + segm->file_size, 0, (int)(segm->mem_size - segm->file_size));
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, &old_handler);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	fd = open(path, O_RDWR);
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	return -1;
}
