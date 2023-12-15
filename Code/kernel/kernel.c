/*
 * boot.c - a kernel template (Assignment 1, CSE 511)
 * Copyright 2022 Ruslan Nikolaev <rnikola@psu.edu>
 */


//void kernel_start(void *kstack, void *ustack, void *fb, int width, int height,
		  //void *ucode, void *memory, unsigned long memorySize)
void kernel_start(unsigned int *framebuffer, int width, int height)
{
    for(int i = 0; i < height/2; i++)
	{
		for(int w = 0; w < width/2; w++)
		{
			(framebuffer)[i + width * w] = 0xFFFFFFFF;
		}
	}
	/* Never exit! */
	while (1) {};
}