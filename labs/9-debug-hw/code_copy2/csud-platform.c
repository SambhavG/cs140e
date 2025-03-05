#include <rpi.h>
#include "csud/include/types.h"

// Implement the memory allocation functions
void* MemoryAllocate(u32 size) {
    return kmalloc(size);
}

void MemoryDeallocate(void* address) {
}

void MemoryCopy(void* destination, void* source, u32 length) {
    memcpy(destination, source, length);
}

// void MemoryReserve(void* destination, void* source, u32 length) {
//     memcpy(destination, source, length);
// }

void* MemoryReserve(u32 length, void* physicalAddress) {
  return physicalAddress;
}


// Implement the logging function
void LogPrint(const char* message, ...) {
    va_list args;
    va_start(args, message);
    printk(message, args);
    va_end(args);
}

// Implement the delay function
void MicroDelay(u32 delay) {
    delay_us(delay);
}

// Implement the USB power function
void PowerOnUsb() {
    // This is platform-specific, but for RPi we can just print a message
    // as the USB is likely already powered
    printk("Powering on USB...\n");
}

// Division functions that might be needed
u32 __aeabi_uidiv(u32 numerator, u32 denominator) {
    return numerator / denominator;
}

void __aeabi_uidivmod(u32 numerator, u32 denominator, u32* quotient, u32* remainder) {
    *quotient = numerator / denominator;
    *remainder = numerator % denominator;
} 