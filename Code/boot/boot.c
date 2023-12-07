/*
 * boot.c - a boot loader template (Assignment 1, CSE 511)
 * Copyright 2022 Ruslan Nikolaev <rnikola@psu.edu>
 */

#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <stdio.h>

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

static void CloseKernel(EFI_FILE_PROTOCOL *vh, EFI_FILE_PROTOCOL *fh)
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
typedef void (*kernel_entry_t) (unsigned int *, int, int) __attribute__((sysv_abi));

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE *systemTable)
{

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
	UINTN tempLength = numToRead;
	char *tempBuffer = (char *)kernelBuffer;
	// while(numToRead > 0)
	// {
	// 	tempLength = numToRead;

		// Read kernal to buffer
		efi_status = fh->Read(fh, &tempLength, tempBuffer);
		if (EFI_ERROR(efi_status)) {
			SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to read kernal!\r\n");
			
			return efi_status;
		}

	// 	// Update numToRead
	// 	numToRead -= tempLength;
	// 	tempBuffer += tempLength;
	// }


	CloseKernel(vh, fh);

	/////////////////
	//Part 2:
	
    EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;
    UINTN mapKey, mapDescriptorSize;
	UINTN mapSize = 0;
    UINT32 mapDescriptorVersion;

    // Retrieve memory map size
    efi_status = BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &mapDescriptorSize, &mapDescriptorVersion);
    if (efi_status != EFI_BUFFER_TOO_SMALL) {
        FreePool(kernelBuffer); // Free allocated buffer
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to retrieve memory map size!\r\n");
        return efi_status;
    }

	do{
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"checkpoint 1\r\n");

		//Free memoryMap buffer if already allocated
		if(memoryMap != NULL)
			FreePool(memoryMap);
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"checkpoint 2\r\n");

		// Allocate memory for memoryMap
		memoryMap = AllocatePool(mapSize);
		if(memoryMap == NULL) // Return error if allocation was unsuccessful 
		{
			FreePool(kernelBuffer); // Free allocated buffer
			SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to allocate memoryMap buffer!\r\n");
			return RETURN_OUT_OF_RESOURCES;
		}
		SystemTable->ConOut->OutputString(SystemTable->ConOut, L"checkpoint 3\r\n");

		// Retrieve memory map
		efi_status = BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &mapDescriptorSize, &mapDescriptorVersion);
		if (efi_status != EFI_SUCCESS) { // Return error if retrieval not successful 
			FreePool(kernelBuffer); // Free allocated buffers
			FreePool(memoryMap);
			SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to retrieve memoryMap!\r\n");
			return efi_status;
		}
		//SystemTable->ConOut->OutputString(SystemTable->ConOut, L"checkpoint 4\r\n");

		if(efi_status == EFI_SUCCESS)
		{
			efi_status = BootServices->ExitBootServices(imageHandle, mapKey);
			if((efi_status != EFI_INVALID_PARAMETER) && (EFI_ERROR(efi_status))) {// Return error if exit returned unsafe  was unsuccessful 
				FreePool(kernelBuffer); // Free allocated buffers
				FreePool(memoryMap);
				SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to exit boot services!\r\n");
				return efi_status;
			}
		}
	}while(efi_status != EFI_SUCCESS);
	
	FreePool(memoryMap); //Free allocated memoryMap buffer
	/////////////////

	fb = SetGraphicsMode(800, 600);

	kernel_entry_t func = (kernel_entry_t) kernelBuffer;

	if (fb == NULL) {
		// Handle error
		FreePool(kernelBuffer);
		return RETURN_UNSUPPORTED;
	}

	func(fb, 800, 600);
	
	FreePool(kernelBuffer);
	return EFI_SUCCESS;
}
