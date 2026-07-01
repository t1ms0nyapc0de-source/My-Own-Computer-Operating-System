CC = gcc
LD = ld
AS = nasm

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-builtin -fno-stack-protector -nostdlib -Isrc
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T src/linker.ld -nostdlib

C_SOURCES = src/kernel.c src/gdt.c src/idt.c src/syscall.c src/pmm.c src/vmm.c src/task.c src/timer.c src/vfs.c src/tar.c src/elf.c src/string.c src/terminal.c
ASM_SOURCES = src/boot.s src/gdt_flush.s src/isr.s src/switch.s src/user_switch.s

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

all: myos.iso

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

hello: src/test_program.c
	$(CC) -m32 -ffreestanding -nostdlib -no-pie -e main -Wl,-Ttext,0x08048000 $< -o $@

initrd.tar: hello
	tar -cf initrd.tar --format=ustar hello

myos.bin: $(OBJECTS) src/linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o myos.bin

myos.iso: myos.bin initrd.tar grub.cfg
	mkdir -p iso/boot/grub
	cp myos.bin iso/boot/
	cp initrd.tar iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o myos.iso iso

clean:
	rm -rf $(OBJECTS) myos.bin initrd.tar hello myos.iso iso
