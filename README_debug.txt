DEBUG con EMULATORE

COMANDO 1

 kernel-qemu      the emulator-specific Linux kernel image
    ramdisk.img      the ramdisk image used to boot the system
    system.img       the *initial* system image
    userdata.img     the *initial* data partition image


.~/android-sdk-linux/tools/emulator-arm -show-kernel 

# -sysdir <dir> search for system disk images in <dir>
# -system <file>  read initial system image from <file>


-system out/target/

-kernel kernel/arch/arm/boot/zImage

-logcat *:v 
-qemu -monitor telnet::4444,server -s 

COMANDO 2
toolchain/arm-eabi-4.2.1/bin/arm-eabi-gdb 
kernel/vmlinux 

COMANDO 3
telnet localhost 4444 
