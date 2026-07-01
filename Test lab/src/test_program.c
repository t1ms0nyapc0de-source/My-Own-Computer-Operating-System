#include <stdint.h>

static inline int syscall_write(const char* str, uint32_t len) {
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(0), "c"((uint32_t)str), "d"((uint32_t)len)
        : "memory"
    );
    return ret;
}

static inline void syscall_exit(int code) {
    asm volatile(
        "int $0x80"
        :
        : "a"(1), "b"((uint32_t)code)
    );
}

void main(void) {
    const char* msg = "\n[User ELF] Hello World from Ring 3! Executing ELF loaded dynamically from TAR RAM disk.\n";
    uint32_t len = 0;
    while (msg[len]) len++;
    
    syscall_write(msg, len);

    const char* msg2 = "[User ELF] Program finished. Calling exit syscall with code 5.\n";
    uint32_t len2 = 0;
    while (msg2[len2]) len2++;

    syscall_write(msg2, len2);
    
    syscall_exit(5);
}
