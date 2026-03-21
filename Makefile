K=kernel
U=user
B=bin

OBJS = \
  $B/$K/entry.o \
  $B/$K/start.o \
  $B/$K/console.o \
  $B/$K/printf.o \
  $B/$K/uart.o \
  $B/$K/kalloc.o \
  $B/$K/spinlock.o \
  $B/$K/string.o \
  $B/$K/main.o \
  $B/$K/vm.o \
  $B/$K/proc.o \
  $B/$K/swtch.o \
  $B/$K/trampoline.o \
  $B/$K/trap.o \
  $B/$K/syscall.o \
  $B/$K/sysproc.o \
  $B/$K/bio.o \
  $B/$K/fs.o \
  $B/$K/log.o \
  $B/$K/sleeplock.o \
  $B/$K/file.o \
  $B/$K/pipe.o \
  $B/$K/exec.o \
  $B/$K/sysfile.o \
  $B/$K/kernelvec.o \
  $B/$K/plic.o \
  $B/$K/virtio_disk.o

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-elf-'; \
	elif riscv64-none-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-none-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64
MIN_QEMU_VERSION = 7.2

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -Wno-unknown-attributes -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -march=rv64gc
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding
CFLAGS += -fno-common -nostdlib
CFLAGS += -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset
CFLAGS += -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-log -fno-builtin-bzero
CFLAGS += -fno-builtin-strchr -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc
CFLAGS += -fno-builtin-free
CFLAGS += -fno-builtin-memcpy -Wno-main
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

$B/$K/kernel: $(OBJS) $K/kernel.ld
	mkdir -p $B/$K
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $B/$K/kernel $(OBJS) 
	$(OBJDUMP) -S $B/$K/kernel > $B/$K/kernel.asm
	$(OBJDUMP) -t $B/$K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $B/$K/kernel.sym

$B/$K/%.o: $K/%.c
	mkdir -p $B/$K
	$(CC) $(CFLAGS) -c -o $@ $<

$B/$K/entry.o: $K/entry.S
	mkdir -p $B/$K
	$(CC) -march=rv64gc -g -c -o $@ $<

$B/$K/swtch.o: $K/swtch.S
	mkdir -p $B/$K
	$(CC) -march=rv64gc -g -c -o $@ $<

$B/$K/trampoline.o: $K/trampoline.S
	mkdir -p $B/$K
	$(CC) -march=rv64gc -g -c -o $@ $<

$B/$K/kernelvec.o: $K/kernelvec.S
	mkdir -p $B/$K
	$(CC) -march=rv64gc -g -c -o $@ $<

tags: $(OBJS)
	etags kernel/*.S kernel/*.c

ULIB = $B/$U/ulib.o $B/$U/usys.o $B/$U/printf.o $B/$U/umalloc.o

$U/_%: $B/$U/%.o $(ULIB) $U/user.ld
	$(LD) $(LDFLAGS) -T $U/user.ld -o $@ $B/$U/$*.o $(ULIB)
	$(OBJDUMP) -S $@ > $B/$U/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $B/$U/$*.sym

$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S

$B/$U/usys.o : $U/usys.S
	mkdir -p $B/$U
	$(CC) $(CFLAGS) -c -o $B/$U/usys.o $U/usys.S

$B/$U/%.o: $U/%.c
	mkdir -p $B/$U
	$(CC) $(CFLAGS) -c -o $@ $<

$U/_forktest: $B/$U/forktest.o $B/$U/ulib.o $B/$U/usys.o
	mkdir -p $B/$U
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest $B/$U/forktest.o $B/$U/ulib.o $B/$U/usys.o
	$(OBJDUMP) -S $U/_forktest > $B/$U/forktest.asm

mkfs/mkfs: mkfs/mkfs.c $K/fs.h $K/param.h
	gcc -Wno-unknown-attributes -I. -o mkfs/mkfs mkfs/mkfs.c

UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
	$U/_logstress\
	$U/_forphan\
	$U/_dorphan\
	$U/_trace\
	$U/_sysinfotest\

fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)

-include $B/$K/*.d $B/$U/*.d

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	$B/$K/kernel fs.img \
	mkfs/mkfs .gdbinit \
        $U/usys.S \
	$(UPROGS)
	rm -rf $B

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -bios none -kernel $B/$K/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu: check-qemu-version $B/$K/kernel fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $B/$K/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

print-gdbport:
	@echo $(GDBPORT)

QEMU_VERSION := $(shell $(QEMU) --version | head -n 1 | sed -E 's/^QEMU emulator version ([0-9]+\.[0-9]+)\..*/\1/')
check-qemu-version:
	@if [ "$(shell echo "$(QEMU_VERSION) >= $(MIN_QEMU_VERSION)" | bc)" -eq 0 ]; then \
		echo "ERROR: Need qemu version >= $(MIN_QEMU_VERSION)"; \
		exit 1; \
	fi
