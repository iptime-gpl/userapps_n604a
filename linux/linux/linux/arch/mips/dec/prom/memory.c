/*
 * memory.c: memory initialisation code.
 *
 * Copyright (C) 1998 Harald Koerfgen, Frieder Streffer and Paul M. Antoine
 * Copyright (C) 2000, 2002  Maciej W. Rozycki
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/types.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/dec/machtype.h>
#include <asm/dec/prom.h>
#include <asm/page.h>


volatile unsigned long mem_err = 0;	/* So we know an error occurred */

/*
 * Probe memory in 4MB chunks, waiting for an error to tell us we've fallen
 * off the end of real memory.  Only suitable for the 2100/3100's (PMAX).
 */

#define CHUNK_SIZE 0x400000

static inline void pmax_setup_memory_region(void)
{
	volatile unsigned char *memory_page, dummy;
	char old_handler[0x80];
	extern char genexcept_early;

	/* Install exception handler */
	memcpy(&old_handler, (void *)(KSEG0 + 0x80), 0x80);
	memcpy((void *)(KSEG0 + 0x80), &genexcept_early, 0x80);

	for (memory_page = (unsigned char *) KSEG1 + CHUNK_SIZE;
	     (mem_err== 0) && (memory_page < ((unsigned char *) KSEG1+0x1E000000));
  	     memory_page += CHUNK_SIZE) {
		dummy = *memory_page;
	}
	memcpy((void *)(KSEG0 + 0x80), &old_handler, 0x80);

	add_memory_region(0, (unsigned long)memory_page - KSEG1 - CHUNK_SIZE,
			  BOOT_MEM_RAM);
}

/*
 * Use the REX prom calls to get hold of the memory bitmap, and thence
 * determine memory size.
 */
static inline void rex_setup_memory_region(void)
{
	int i, bitmap_size;
	unsigned long mem_start = 0, mem_size = 0;
	memmap *bm;

	/* some free 64k */
	bm = (memmap *)KSEG0ADDR(0x28000);

	bitmap_size = rex_getbitmap(bm);

	for (i = 0; i < bitmap_size; i++) {
		if (bm->bitmap[i] == 0xff)
			mem_size += (8 * bm->pagesize);
		else if (!mem_size)
			mem_start += (8 * bm->pagesize);
		else {
			add_memory_region(mem_start, mem_size, BOOT_MEM_RAM);
			mem_start += mem_size + (8 * bm->pagesize);
			mem_size = 0;
		}
	}
	if (mem_size)
		add_memory_region(mem_start, mem_size, BOOT_MEM_RAM);
}

void __init prom_meminit(u32 magic)
{
	if (!prom_is_rex(magic))
		pmax_setup_memory_region();
	else
		rex_setup_memory_region();
}

void __init prom_free_prom_memory (void)
{
	unsigned long addr, end;
	extern char _ftext;

	/*
	 * Free everything below the kernel itself but leave
	 * the first page reserved for the exception handlers.
	 */

#if defined(CONFIG_DECLANCE) || defined(CONFIG_DECLANCE_MODULE)
	if (IOASIC)
		end = __pa(&_ftext) - 0x00020000;
	else
#endif
		end = __pa(&_ftext);

	addr = PAGE_SIZE;
	while (addr < end) {
		ClearPageReserved(virt_to_page(__va(addr)));
		set_page_count(virt_to_page(__va(addr)), 1);
		free_page((unsigned long)__va(addr));
		addr += PAGE_SIZE;
	}

	printk("Freeing unused PROM memory: %ldk freed\n",
	       (end - PAGE_SIZE) >> 10);
}