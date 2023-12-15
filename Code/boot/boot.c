/*
 * boot.c - a boot loader template (Assignment 1, CSE 511)
 * Copyright 2022 Ruslan Nikolaev <rnikola@psu.edu>
 */

#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>


/* Use GUID names 'gEfi...' that are already declared in Protocol headers. */
EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

/* Keep these variables global. */
static EFI_HANDLE ImageHandle;
static EFI_SYSTEM_TABLE *SystemTable;
static EFI_BOOT_SERVICES *BootServices;

// Currently uses only EfiBootServicesData
// The function can be extended to accept any type as necessary
static VOID *AllocatePool(UINTN size)
{
	VOID *ptr;
	EFI_STATUS ret = BootServices->AllocatePool(EfiBootServicesData, size, &ptr);
	if (EFI_ERROR(ret))
		return NULL;
	return ptr;
}

static VOID FreePool(VOID *buf)
{
	BootServices->FreePool(buf);
}
static VOID FreePages(EFI_PHYSICAL_ADDRESS *luf, UINTN pages)
{
	BootServices->FreePages(*luf, pages);
}
static EFI_STATUS OpenKernel(EFI_FILE_PROTOCOL **pvh, EFI_FILE_PROTOCOL **pfh)
{
	EFI_LOADED_IMAGE *li = NULL;
	EFI_FILE_IO_INTERFACE *fio = NULL;
	EFI_FILE_PROTOCOL *vh;
 	EFI_STATUS efi_status;

	*pvh = NULL;
	*pfh = NULL;

	efi_status = BootServices->HandleProtocol(ImageHandle,
					&gEfiLoadedImageProtocolGuid, (void **) &li);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
			L"Cannot get LoadedImage for BOOTx64.EFI\r\n");
		return efi_status;
	}

	efi_status = BootServices->HandleProtocol(li->DeviceHandle,
					&gEfiSimpleFileSystemProtocolGuid, (void **) &fio);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
						L"Cannot get fio\r\n");
		return efi_status;
	}

	efi_status = fio->OpenVolume(fio, pvh);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
						L"Cannot get the volume handle!\r\n");
		return efi_status;
	}
	vh = *pvh;

	efi_status = vh->Open(vh, pfh, L"\\EFI\\BOOT\\KERNEL",
					EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
						L"Cannot get the file handle!\r\n");
		return efi_status;
	}

	return EFI_SUCCESS;
}

static EFI_STATUS OpenUser(EFI_FILE_PROTOCOL **pvh, EFI_FILE_PROTOCOL **pfh)
{
	EFI_LOADED_IMAGE *li = NULL;
	EFI_FILE_IO_INTERFACE *fio = NULL;
	EFI_FILE_PROTOCOL *vh;
 	EFI_STATUS efi_status;

	*pvh = NULL;
	*pfh = NULL;

	efi_status = BootServices->HandleProtocol(ImageHandle,
					&gEfiLoadedImageProtocolGuid, (void **) &li);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
			L"Cannot get LoadedImage for BOOTx64.EFI\r\n");
		return efi_status;
	}

	efi_status = BootServices->HandleProtocol(li->DeviceHandle,
					&gEfiSimpleFileSystemProtocolGuid, (void **) &fio);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
						L"Cannot get fio\r\n");
		return efi_status;
	}

	efi_status = fio->OpenVolume(fio, pvh);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
						L"Cannot get the volume handle!\r\n");
		return efi_status;
	}
	vh = *pvh;

	efi_status = vh->Open(vh, pfh, L"\\EFI\\BOOT\\USER",
					EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
						L"Cannot get the user file handle!\r\n");
		return efi_status;
	}

	return EFI_SUCCESS;
}

static void CloseKernel(EFI_FILE_PROTOCOL *vh, EFI_FILE_PROTOCOL *fh)
{
	vh->Close(vh);
	fh->Close(fh);
}

static void CloseUser(EFI_FILE_PROTOCOL *vh, EFI_FILE_PROTOCOL *fh)
{
	vh->Close(vh);
	fh->Close(fh);
}

static UINT32 *SetGraphicsMode(UINT32 width, UINT32 height)
{
	EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics;
	EFI_STATUS efi_status;
	UINT32 mode;

	efi_status = BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
                    NULL, (VOID **) &graphics);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
			L"Cannot get the GOP handle!\r\n");
		return NULL;
	}

	if (!graphics->Mode || graphics->Mode->MaxMode == 0) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut,
			L"Incorrect GOP mode information!\r\n");
		return NULL;
	}

	for (mode = 0; mode < graphics->Mode->MaxMode; mode++) {
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
		UINTN size;

		efi_status = graphics->QueryMode(graphics, mode, &size, &info);

        if (EFI_ERROR(efi_status))
        {
            SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                              L"Failed to query graphics mode!\r\n");
            return NULL;
        }

		// TODO: Locate BlueGreenRedReserved, aka BGRA (8-bit per color)
		// Resolution width x height (800x600 in our code)
        if (info->HorizontalResolution == width &&
            info->VerticalResolution == height &&
            info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor)
        {
            // Activate (set) this graphics mode
            efi_status = graphics->SetMode(graphics, mode);

            if (EFI_ERROR(efi_status))
            {
                SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                                  L"Failed to set graphics mode!\r\n");
                return NULL;
            }
				// Return the frame buffer base address
			return (UINT32 *) graphics->Mode->FrameBufferBase;
		}

	}
	return NULL;
}

/* Use System V ABI rather than EFI/Microsoft ABI. */
//typedef void (*kernel_entry_t) (void *, void *, void *, int, int, void *, void *, unsigned long) __attribute__((sysv_abi));
typedef void (*kernel_entry_t) (unsigned int *, int, int) __attribute__((sysv_abi));
EFI_STATUS EFIAPI
efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE *systemTable)
{
	///////////////////////
	// Part 1: 
	EFI_FILE_PROTOCOL *vh, *fh;
	EFI_STATUS efi_status;
	UINT32 *fb;
	
	ImageHandle = imageHandle;
	SystemTable = systemTable;
	BootServices = systemTable->BootServices;

	efi_status = OpenKernel(&vh, &fh);

	if (EFI_ERROR(efi_status)) {
		BootServices->Stall(5 * 1000000); // 5 seconds
		return efi_status;
	}

	// Load the kernel
    // Seek to the beginning of the file
    efi_status = fh->SetPosition(fh, 0);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to load kernal!\r\n");
		return efi_status;
    }

	//Allocate kernal buffer 
	void *kernelBuffer = AllocatePool(SIZE_1MB);
	if (kernelBuffer == NULL) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to allocate kernal buffer!\r\n");
        return RETURN_OUT_OF_RESOURCES;
    }

    // Read the file content into the buffer
	UINTN numToRead = SIZE_1MB;

	efi_status = fh->Read(fh, &numToRead, kernelBuffer);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to read kernal!\r\n");
		return efi_status;
	}

	CloseKernel(vh, fh);

	fb = SetGraphicsMode(800, 600);
	if (fb == NULL) {
		// Handle error
		FreePool(kernelBuffer);
		return RETURN_UNSUPPORTED;
	}

	kernel_entry_t func = (kernel_entry_t) kernelBuffer;

	/////////////////////////////////////
	//Part 3:
	
	EFI_FILE_PROTOCOL *u_vh, *u_fh;

	efi_status = OpenUser(&u_vh, &u_fh);

		if (EFI_ERROR(efi_status)) {
		BootServices->Stall(5 * 1000000); // 5 seconds
		return efi_status;
	}

	// Load the User
    // Seek to the beginning of the file
    efi_status = fh->SetPosition(fh, 0);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to load user!\r\n");
		return efi_status;
    }

	//Allocate user buffer 
	void *ucode = AllocatePool(SIZE_1MB);
	if (ucode == NULL) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to allocate user buffer!\r\n");
        return RETURN_OUT_OF_RESOURCES;
    }

    // Read the file content into the buffer
	numToRead = SIZE_1MB;

	efi_status = fh->Read(fh, &numToRead, ucode);
	if (EFI_ERROR(efi_status)) {
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to read user!\r\n");
		return efi_status;
	}

	CloseUser(u_vh, u_fh);



	//Allocate memory to be passed into kernal
	EFI_PHYSICAL_ADDRESS *memoryBuffer;
	UINTN memoryBufferSize = (SIZE_1MB * 32);
	UINTN num_pages = (memoryBufferSize / EFI_PAGE_SIZE);

	efi_status = BootServices->AllocatePages(AllocateAnyPages, EfiBootServicesData, num_pages, memoryBuffer);
	if(efi_status != EFI_SUCCESS)
	{
		FreePool(ucode);
		FreePool(kernelBuffer);
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to allocate pages!\r\n");
		return efi_status;
	}

	//Allocate ustack
	EFI_PHYSICAL_ADDRESS *ustack;
	efi_status = BootServices->AllocatePages(AllocateAnyPages, EfiBootServicesData, 1, ustack);
	if(efi_status != EFI_SUCCESS)
	{
		FreePool(ucode);
		FreePages(memoryBuffer, num_pages);
		FreePool(kernelBuffer);
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to allocate pages!\r\n");
		return efi_status;
	}
	//Set pointer to end of buffer
	ustack = (EFI_PHYSICAL_ADDRESS *)((void *)ustack + EFI_PAGE_SIZE);

	//Allocate kstack
	EFI_PHYSICAL_ADDRESS *kstack;
	efi_status = BootServices->AllocatePages(AllocateAnyPages, EfiBootServicesData, 1, kstack);
	if(efi_status != EFI_SUCCESS)
	{
		FreePages(ustack, 1);
		FreePool(ucode);
		FreePages(memoryBuffer, num_pages);
		FreePool(kernelBuffer);
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to allocate pages!\r\n");
		return efi_status;
	}
	//Set pointer to end of buffer
	kstack = (EFI_PHYSICAL_ADDRESS *)((void *)kstack + EFI_PAGE_SIZE);

	/////////////////
	//Part 2:
	
    EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
    UINTN mapKey, mapDescriptorSize;
	UINTN mapSize = 0;
    UINT32 mapDescriptorVersion;

	do{
		mapSize = 0;
		memoryMap = NULL;

		// Retrieve memory map size
		efi_status = BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &mapDescriptorSize, &mapDescriptorVersion);
		if (efi_status != EFI_BUFFER_TOO_SMALL) {
			FreePages(kstack, 1);
			FreePages(ustack, 1);
			FreePool(ucode);
			FreePages(memoryBuffer, num_pages);
			FreePool(kernelBuffer); // Free allocated buffers
			SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to retrieve memory map size!\r\n");
			return efi_status;
		}

		// Allocate memory for memoryMap
		memoryMap = AllocatePool(mapSize);
		if(memoryMap == NULL) // Return error if allocation was unsuccessful 
		{
			FreePages(kstack, 1);
			FreePages(ustack, 1);
			FreePool(ucode);
			FreePages(memoryBuffer, num_pages);
			FreePool(kernelBuffer); // Free allocated buffers
			SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to allocate memoryMap buffer!\r\n");
			return RETURN_OUT_OF_RESOURCES;
		}

		// Retrieve memory map
		efi_status = BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &mapDescriptorSize, &mapDescriptorVersion);
		if (efi_status != EFI_SUCCESS) { // Return error if retrieval not successful 
			FreePages(kstack, 1);
			FreePages(ustack, 1);
			FreePool(ucode);
			FreePages(memoryBuffer, num_pages);
			FreePool(kernelBuffer);
			FreePool(memoryMap); // Free allocated buffers
			SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to retrieve memoryMap!\r\n");
			return efi_status;
		}

		// Try to call ExitBootServices if memory map retrieval is successful
		if(efi_status == EFI_SUCCESS)
		{
			efi_status = BootServices->ExitBootServices(imageHandle, mapKey);
			if(efi_status != EFI_SUCCESS) {// Return error if exit was unsuccessful 
				FreePool(memoryMap);
			}
		}
	}while(efi_status != EFI_SUCCESS);
	
	/////////////////
	//func((void *) kstack, (void *) ustack, (void *) fb, 800, 600, (void *) ucode, (void *) memoryBuffer, memoryBufferSize);
	func(fb, 800, 600);
	return EFI_SUCCESS;
}
