/*
 * kernel_extra.c - Project 2 Extra Credit, CMPSC 473
 * Copyright 2023 Ruslan Nikolaev <rnikola@psu.edu>
 * Distribution, modification, or usage without explicit author's permission
 * is not allowed.
 */

#include <malloc.h>
#include <types.h>
#include <string.h>
#include <printf.h>

// Your mm_init(), malloc(), free() code from mm.c here
// You can only use mem_sbrk(), mem_heap_lo(), mem_heap_hi() and
// Project 2's kernel headers provided in 'include' such
// as memset and memcpy.
// No other files from Project 1 are allowed!

#define ALIGNMENT 16

///////////////////////////////////////////

/*Global Variables*/

//Initial size of heap
const size_t initHeapSize = 4096 + 8;

//Pointer to first header, last footer, and last header in heap
size_t *firstHeader = NULL;
size_t *lastFooter = NULL;
size_t *lastBlockHeader = NULL;

typedef struct header header;

//Header for Free Blocks
struct header{
    size_t size;
    header *next;
    header *prev;
};

//Segregated Free List
header *segList[64];
/////////////////////////////////////////////

//Return Log2 of size for Seg List Mapping
static size_t int_log2 (size_t size)
{
    int clz = __builtin_clzl(size);
    return (sizeof(size_t) * 8 - 1) ^ clz;
}

//Build header/footer with block size, allocation bit, and prev allocation bit
size_t setup_header_footer(size_t size, bool isAllocated, bool prevAllocated)
{
    uint64_t output = 0;

    // Add data size and allocation bit to output
    output = size;
    output |= (isAllocated & 1);

    if(prevAllocated)
        output |= 2;

    return output;
}

//Build Free Block header with block size, allocation bit, prev allocation bit, 
//next free block pointer, and prev free block pointer.
//Next and Prev initialized as NULL.
header setup_freeBlock(size_t size, bool isAllocated, bool prevAllocated)
{
    header blockHeader;
    blockHeader.size = setup_header_footer(size, isAllocated, prevAllocated);
    blockHeader.next = NULL;
    blockHeader.prev = NULL;
    return blockHeader;

}

//Looks for a free block by iterating through seg list and finds best fit
header *getFreeHeader(size_t size) 
{
    size_t index = int_log2(size);
    header *bestFitHeader = NULL;   // This will store the best fit block we find
    size_t bestFitSize = SIZE_MAX;  // Initialize with maximum possible size

    // Iterate through seg free list size classes untill best fit is found
    while(index < 64 && bestFitHeader == NULL)
    {
        header* freeHeader = segList[index];
        while(freeHeader != NULL)
        {
            size_t currentBlockSize = freeHeader->size & -4;
            // If this block is a better fit than our current best fit, update bestFitHeader and bestFitSize
            if (currentBlockSize >= size)
            {
                // Continue to next block in size class if block is not a better fit
                if((bestFitHeader != NULL) && currentBlockSize >= bestFitSize)
                {
                    freeHeader = freeHeader->next;
                    continue;
                }

                //Update best fit
                bestFitSize = currentBlockSize;
                bestFitHeader = freeHeader;

                // If the block size matches the requested size exactly, break out of the loop
                if (bestFitSize == size)
                    break;
            }
            //Proceed to next free block in current size class
            freeHeader = freeHeader->next;
        }
        //Proceed to next seg list size class
        index++;
    }

    return bestFitHeader; // Return the best fit block found, or NULL if none found
}

//Remove passed in block from the free list
void remove_from_free_list(header *block)
{
    //Determine the seg list size class
    int index = int_log2(block->size & -4);

    //If the block has a previous block, update the next pointer of the previous block
    if (block->prev != NULL)
    {
        block->prev->next = block->next;
    }
    else
    {
        //If block is first in the list, update the seg list's head to be the next block
        segList[index] = block->next;
    }

    //If the block has a next block, update the previous pointer of the next block
    if (block->next != NULL)
    {
        block->next->prev = block->prev;
    }
}

//Add passed in block to the free list
void add_to_free_list(header *block)
{
    //Determine the seg list size class
    size_t index = int_log2(block->size & -4);

    //Set the next pointer of the block to the current head of the seg list
    block->next = segList[index];

    //Set the previous pointer of the block to NULL since it will be the new head
    block->prev = NULL;

    //If the seg list is not empty, set the previous pointer of the current head to the new block
    if(segList[index] != NULL)
    {
        segList[index]->prev = block;
    }

    //Update the seg list's head to the new block
    segList[index] = block;
}


/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

//Aligns size to neart multiple of 8, that is not a multiple of 16
static size_t align8(size_t x)
{
    size_t alginedSize = align(x);
    if(x > (alginedSize - 8))
        return (alginedSize + 8);
    else 
        return (alginedSize - 8);
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    //Reset heap
    //mem_reset_brk();

    //Initialize seg list to NULL
    for (int i = 0; i < 64; i++) {
        segList[i] = NULL;
    }

    //Allocate inital memory of heap and check if successful
    if(mem_sbrk(initHeapSize) == (void *)-1)
    {
        return false;
    }

    //Set inital header and footer postions of the heap
    firstHeader = mem_heap_lo() + sizeof(size_t);                               
    *((header *)firstHeader) = setup_freeBlock(initHeapSize - sizeof(size_t), false, true);   
    size_t *lastFreeBlock = mem_heap_lo() + initHeapSize - sizeof(size_t);                
    *lastFreeBlock = setup_header_footer(initHeapSize - sizeof(size_t), false, true);
    
    //Update last block header to first block
    lastBlockHeader = firstHeader;

    //Add first header to free list
    add_to_free_list((header *)firstHeader);
    return true;
}

/*f
 * malloc
 */
void* malloc(size_t size)
{  
    //Return NULL if not allocating memory
    if(size == 0)
        return NULL;

    //Align inputed size
    size_t alignedSize = align8(size);

    //Determine new block size
    size_t newSize = alignedSize + sizeof(size_t);      //size of block = payload_size + header_size 

    //Round block size to minimum size of free block if less than minimum
    if(newSize < (sizeof(header) + sizeof(size_t)))
    {
        newSize = (sizeof(header) + sizeof(size_t));
    }

    //Find a free block
    header *freeHeader = getFreeHeader(newSize);

    //Allocate more heap space if getFreeHeader failed
    if(freeHeader == NULL)
    {
        //Extend heap
        freeHeader = mem_sbrk(newSize);

        //If sbrk fails return NULL
        if(freeHeader == (void *)-1)
        {
            return NULL;
        }

        //Update free block header to be allocated with new size, 
        //and set prev allocation bit based on last block allocation bit
        *((size_t *)freeHeader) = setup_header_footer(newSize, true, (bool)(*lastBlockHeader & 1)); 

        //Update last block header to be new allocated block
        lastBlockHeader = (size_t *)freeHeader;
    }
    else{
        //Size of found free block
        size_t oldSize = (freeHeader->size  & -4);

        //Remove found free block from free list
        remove_from_free_list(freeHeader);

        //Split found free block if it is larger than the required size
        if(newSize < oldSize) 
        {
            //Determine new free block size by taking difference of current free block size and size you want to allocate
            size_t sizeDiff = oldSize - newSize;

            //Check if new free block is big enough to store free block data
            if (sizeDiff >= (sizeof(header) + sizeof(size_t))) 
            {
                //Setup new free block header and footer
                header *newFree = (header *)((char *)freeHeader + newSize); 
                *newFree = setup_freeBlock(sizeDiff, false, true); 
                size_t *newFreeFooter = (size_t *)((char *)newFree + sizeDiff - sizeof(size_t)); 
                *newFreeFooter = setup_header_footer(sizeDiff, false, true);

                //Add new free block to the free list
                add_to_free_list(newFree);

                //Update last block header if free header is current last block 
                if((size_t *)freeHeader == lastBlockHeader)
                {
                    lastBlockHeader = (size_t *)newFree;
                }
            }
            else //Use entire current free block
            {
                newSize = oldSize;

                //Update next block prev allocated bit to true
                size_t *nextHeader = (size_t *)((char *)freeHeader + newSize);
                if(nextHeader < (size_t *)(mem_heap_hi() + 1) - 1)
                {
                    *nextHeader |= 2;
                }
            }
        }
        else //Use entire current free block
        {
            newSize = oldSize;

            //Update next block prev allocated bit to true
            size_t *nextHeader = (size_t *)((char *)freeHeader + newSize);
            if(nextHeader < (size_t *)(mem_heap_hi() + 1) - 1)
            {
                *nextHeader |= 2;
            }
        }

        //Update free block header to be allocated with new size, and set prev allocation bit true
        *((size_t *)freeHeader) = setup_header_footer(newSize, true, true); 
    }

    //Return pointer to payload
    return (char *)freeHeader + sizeof(size_t);
}

/*
 * free
 */
void free(void* ptr)
{
    //Return nothing if pointer is NULL   
    if(ptr == NULL)
    {
        return;
    }

    //Update last footer in heap
    lastFooter = mem_heap_hi() + 1 - sizeof(size_t);
    
    //Determine current payload's header and footer
    size_t* ptrHeader = (size_t*)ptr - 1;
    size_t* ptrFooter = ((size_t*)((char*)ptrHeader + (*ptrHeader & -4))) - 1;

    //Determine header and footer of block in front of payload
    size_t* frontHeader = ptrFooter + 1;
    size_t* frontFooter = ((size_t*)((char*)frontHeader + (*frontHeader & -4))) - 1;

    //Determine header and footer of block behind the payload
    size_t* backFooter = ptrHeader - 1;
    size_t* backHeader = ((size_t*)((char*)backFooter - (*backFooter & -4))) + 1;

    //Determine if block in front and back of payload are free
    // true = free, flase = not free
    bool backFree = false;
    bool frontFree = false;

    //Check if back is free and vaild
    if(!(bool)(*ptrHeader & 2) && backFooter >= firstHeader)
    {
        backFree =  !(bool)(*ptrHeader & 2);        
    }

    //Check if front is free and valid
    if(frontHeader <= lastFooter)
    {
        frontFree = !(bool)(*frontHeader & 1);       
    }

    //New free block size
    size_t newSize = 0; 
    
    //Coalescing
    //If no free block in the front and back of payload
    if(!backFree && !frontFree) 
    {
        //New free block size = payload block size
        newSize = (*ptrHeader & -4);

        //Set free block header and footer, then add to free list
        *((header *)ptrHeader) = setup_freeBlock(newSize, false, true);
        *ptrFooter = setup_header_footer(newSize, false, true);
        add_to_free_list((header *)ptrHeader);

        //Update front block's prev allocated bit to zero
        if(frontHeader < (size_t *)(mem_heap_hi() + 1) - 1)
        {
            *frontHeader &= -3;
        }
    }
    //If only a free block in front of payload
    else if(!backFree && frontFree) 
    {
        //Remove front free block from free list
        remove_from_free_list((header *)frontHeader);

        //New free block size = payload block + front block size
        newSize = (*ptrHeader & -4) + (*frontHeader & -4);

        //Set free block header and footer, then add to free list
        *((header *)ptrHeader) = setup_freeBlock(newSize, false, true);
        *ptrFooter = 0;
        *frontFooter = setup_header_footer(newSize, false, true);
        *frontHeader = 0;
        add_to_free_list((header *)ptrHeader);
        
        //If front block is last block in heap, 
        //then set last block header as new free block
        if(frontHeader == lastBlockHeader)
        {
            lastBlockHeader = ptrHeader;
        }
    }
    //If only a free block behind the payload
    else if(backFree && !frontFree) 
    {
        //Remove back block from free list
        remove_from_free_list((header *)backHeader);

        //New size = payload block + back block size
        newSize = (*ptrHeader & -4) + (*backHeader & -4);

        //Set free block header and footer, then add to free list
        *((header *)backHeader) = setup_freeBlock(newSize, false, true);
        *backFooter = 0;
        *ptrFooter = setup_header_footer(newSize, false, true);
        *ptrHeader = 0;
        add_to_free_list((header *)backHeader);

        //If payload block is last block in heap,
        //then set last block header as new free block
        if(ptrHeader == lastBlockHeader)
        {
            lastBlockHeader = backHeader;
        }

        //Update front block's prev allocated bit to zero
        if(frontHeader < (size_t *)(mem_heap_hi() + 1) - 1)
        {
            *frontHeader &= -3;
        }
    }
    //If free blocks in front and behind the payload
    else  
    {
        //Remove back and front block from the free list
        remove_from_free_list((header *)backHeader);
        remove_from_free_list((header *)frontHeader);

        //New size = payload block + back block + front block size
        newSize = (*ptrHeader & -4) + (*backHeader & -4) + (*frontHeader & -4);

        //Set free block header and footer, then add to free list
        *((header *)backHeader) = setup_freeBlock(newSize, false, true);
        *backFooter = 0;
        *frontFooter = setup_header_footer(newSize, false, true);
        *frontHeader = 0;
        *ptrHeader = 0;
        *ptrFooter = 0;
        add_to_free_list((header *)backHeader);

        //If front block is last block in heap, 
        //then set last block header as new free block
        if(frontHeader == lastBlockHeader)
        {
            lastBlockHeader = backHeader;
        }
    }
    
    return;
}

