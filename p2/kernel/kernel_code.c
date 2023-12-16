/*
 * kernel_code.c - Project 2, CMPSC 473
 * Copyright 2023 Ruslan Nikolaev <rnikola@psu.edu>
 * Distribution, modification, or usage without explicit author's permission
 * is not allowed.
 */

#include <kernel.h>
#include <types.h>
#include <printf.h>
#include <malloc.h>
#include <string.h>

typedef unsigned long long u64;

void *page_table = NULL; 
void *user_stack = NULL; 
void *user_program = NULL;


void kernel_init(void *ustack, void *uprogram, void *memory, size_t memorySize)
{

    //printf("Hello from sgroup69, AmrMantawi\n");

	// 'memory' points to the place where memory can be used to create
	// page tables (assume 1:1 initial virtual-to-physical mappings here)
	// 'memorySize' is the maximum allowed size, do not exceed that (given just in case)

    // Align the memory pointer to the next 4KiB boundary for page table alignment
    u64 *pt = memory;
    // Initialize the Page Table Entries (PTEs)
    for (size_t i = 0; i < 1048576; i++) {
        pt[i] = 0; 			// Clear the entry
        pt[i] |= 3ULL; 		// Set present and writable flags
        pt[i] |= i << 12; 	// Set the physical address assuming 1:1 mapping
    }

    // Initialize the Page Directory (PD) right after the PTEs
    u64 *pd = pt + 1048576;
    for (size_t i = 0; i < 2048; i++) {
        u64 *start_pte = pt + 512 * i;  // Calculate the address of the corresponding PTE
        pd[i] = 0; 						// Clear the entry
        pd[i] |= 3ULL; 					// Set present and writable flags
        pd[i] |= (u64)start_pte; 		// Set the address of the PTE
    }

    // Initialize the Page Directory Pointer Table (PDPT) after the PD
    u64 *pdp = pd + 2048;
    for (size_t i = 0; i < 512; i++) {
        pdp[i] = 0; 					// Clear the entry

        if(i >= 4) 						// Skip entries beyond the fourth
            continue;

        u64 *start_pde = pd + 512 * i;  // Calculate the address of the corresponding PD
        pdp[i] |= 3ULL; 				// Set present and writable flags
        pdp[i] |= (u64)start_pde; 		// Set the address of the PD
    }

    // Initialize the Page Map Level 4 (PML4) after the PDPT
    u64 *pml = pdp + 512;
    for (size_t i = 0; i < 512; i++) {
        pml[i] = 0; 		// Clear the entry

        if(i >= 1) 			// Skip all entries except the first
            continue;

        pml[i] |= 3ULL; 	// Set present and writable flags
        pml[i] |= (u64)pdp; // Set the address of the PDPT
    }

    page_table = pml; // Set the page_table pointer to the PML4

    // Initialize user space page table structures after the kernel page tables
    u64 *u_pt = pml + 512; 		// User space Page Table
    u64 *u_pd = u_pt + 512; 	// User space Page Directory
    u64 *u_pdp = u_pd + 512;	// User space Page Directory Pointer Table

    // Map the last PML4 entry to the user PDPT with user-accessible permissions
    pml[511] = (u64)u_pdp | 7ULL; // 7ULL sets present, writable, and user-accessible flags

    // Initialize the user PDPT
    for (size_t i = 0; i < 512; i++) {
        u_pdp[i] = 0; 			// Clear the entry

        if(i < 511) 			// Skip all entries except the last
            continue;

        u_pdp[i] |= 7ULL; 		// Set present, writable, and user-accessible flags
        u_pdp[i] |= (u64)u_pd;  // Set the address of the user PD
    }

    // Initialize the user PD
    for (size_t i = 0; i < 512; i++) {
        u_pd[i] = 0; 			// Clear the entry

        if(i < 511) 			// Skip all entries except the last
            continue;

        u_pd[i] |= 7ULL; 		// Set present, writable, and user-accessible flags
        u_pd[i] |= (u64)u_pt; 	// Set the address of the user PT
    }

    // Initialize the user PT
    for (size_t i = 0; i < 512; i++) {
        u_pt[i] = 0; 			// Clear the entry
    }

    // Map the user stack and user program physical addresses to the last two entries of the user PT
    u_pt[510] |= ((u64)ustack - 4096ULL) | 7ULL; 	// Map user stack with present, writable, and user-accessible flags
    u_pt[511] |= ((u64)uprogram) | 7ULL; 			// Map user program with present, writable, and user-accessible flags

    // Set the virtual addresses for the user stack and user program
    // Addresses are mapped to the virtual addresses corresponding to the last two entries of the user PT
    user_stack = (void *)(-4096ULL); 	
    user_program = (void *)(-4096ULL); 	

    // The remaining portion just loads the page table,
	// this does not need to be changed:
	// load 'page_table' into the CR3 register
	const char *err = load_page_table(page_table);
	if (err != NULL) {
		printf("ERROR: %s\n", err);
	}

	// The extra credit assignment
	mem_extra_test();
}


/* The entry point for all system calls */
long syscall_entry(long n, long a1, long a2, long a3, long a4, long a5)
{
	// The system call handler to print a message (n = 1)
	// the system call number is in 'n', make sure it is valid!
	if(n == 1)
	{
		char *str = (char *)a1;
		printf("%s\n", str);
		return 0;
	}
	// Arguments are passed in a1,.., a5 and can be of any type
	// (including pointers, which are casted to 'long')
	// For the project, we only use a1 which will contain the address
	// of a string, cast it to a pointer appropriately 

	return -1; /* Success: 0, Failure: -1 */
}
