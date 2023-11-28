BOOT = boot.img
LOADER = boot.efi
LOADER_DLL = boot.dll
KERNEL = kernel.x86_64
UEFI_BIOS = /usr/share/qemu/OVMF.fd

all: $(BOOT)

test: $(BOOT)
	qemu-system-x86_64 -bios $(UEFI_BIOS) -m 1024 -drive format=raw,file=$(BOOT)

$(BOOT): $(KERNEL) $(LOADER)
	@mkdir ./uefi_fat_mnt
	@dd if=/dev/zero of=boot.img bs=1M count=10
	@mkfs.vfat ./boot.img
	@sudo mount -o loop ./boot.img ./uefi_fat_mnt
	@sudo mkdir -p ./uefi_fat_mnt/EFI/BOOT
	@sudo cp $(LOADER) uefi_fat_mnt/EFI/BOOT/BOOTx64.EFI
	@sudo cp $(KERNEL) uefi_fat_mnt/EFI/BOOT/KERNEL
	@sudo umount ./uefi_fat_mnt

# FwImage compilation
FWIMAGE = fwimage/fwimage # Path to the fwimage tool
$(FWIMAGE): fwimage/fwimage.c
	gcc -O3 -Wall -Werror -Wextra -o $@ $<

# Boot loader compilation
CLANG = clang
LLDLINK = lld-link
BOOT_OBJS = boot/boot.o

$(LOADER): $(LOADER_DLL) $(FWIMAGE)
	$(FWIMAGE) app $< $@

$(LOADER_DLL): $(BOOT_OBJS)
	$(LLDLINK) /dll /nodefaultlib /safeseh:no /machine:AMD64 /entry:efi_main $^ /out:$(LOADER_DLL)

boot/%.o: boot/%.c
	$(CLANG) -I./boot/include -I./boot/include/X64 -m64 -O2 -fshort-wchar -mcmodel=small -mno-red-zone -mno-stack-arg-probe -target x86_64-pc-mingw32 -c -o $@ $<

# Kernel compilation
CC = gcc
LD = ld
CFLAGS += -Wall -O1 -mno-red-zone -nostdinc -fno-stack-protector -pie -fno-zero-initialized-in-bss -c # Use -O1 to avoid overoptimization
LDFLAGS = --oformat=binary -T ./kernel/kernel.lds -nostdlib -melf_x86_64 -pie
KERNEL_OBJS = kernel/kernel_entry.o # Do not reorder this one
KERNEL_OBJS += kernel/kernel.o

$(KERNEL): $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

kernel/%.o: kernel/%.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@rm -rf $(KERNEL) $(LOADER) $(LOADER_DLL) *.lib $(BOOT_OBJS) $(KERNEL_OBJS) $(BOOT) $(FWIMAGE) uefi_fat_mnt
