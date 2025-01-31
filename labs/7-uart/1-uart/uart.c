// simple mini-uart driver: implement every routine
// with a <todo>.
//
// NOTE:
//  - from broadcom: if you are writing to different
//    devices you MUST use a dev_barrier().
//  - its not always clear when X and Y are different
//    devices.
//  - pay attenton for errata!   there are some serious
//    ones here.  if you have a week free you'd learn
//    alot figuring out what these are (esp hard given
//    the lack of printing) but you'd learn alot, and
//    definitely have new-found respect to the pioneers
//    that worked out the bcm eratta.
//
// historically a problem with writing UART code for
// this class (and for human history) is that when
// things go wrong you can't print since doing so uses
// uart.  thus, debugging is very old school circa
// 1950s, which modern brains arne't built for out of
// the box.   you have two options:
//  1. think hard.  we recommend this.
//  2. use the included bit-banging sw uart routine
//     to print.   this makes things much easier.
//     but if you do make sure you delete it at the
//     end, otherwise your GPIO will be in a bad state.
//
// in either case, in the next part of the lab you'll
// implement bit-banged UART yourself.
#include "rpi.h"

// change "1" to "0" if you want to comment out
// the entire block.
#if 1
//*****************************************************
// We provide a bit-banged version of UART for debugging
// your UART code.  delete when done!
//
// NOTE: if you call <emergency_printk>, it takes
// over the UART GPIO pins (14,15). Thus, your UART
// GPIO initialization will get destroyed.  Do not
// forget!

// header in <libpi/include/sw-uart.h>
#include "sw-uart.h"
static sw_uart_t sw_uart;

// if we've ever called emergency_printk better
// die before returning.
static int called_sw_uart_p = 0;

// a sw-uart putc implementation.
static int sw_uart_putc(int chr) {
    sw_uart_put8(&sw_uart, chr);
    return chr;
}

// call this routine to print stuff.
//
// note the function pointer hack: after you call it
// once can call the regular printk etc.
static void emergency_printk(const char *fmt, ...) {
    // track if we ever called it.
    called_sw_uart_p = 1;

    // we forcibly initialize each time it got called
    // in case the GPIO got reset.
    // setup gpio 14,15 for sw-uart.
    sw_uart = sw_uart_default();

    // all libpi output is via a <putc>
    // function pointer: this installs ours
    // instead of the default
    rpi_putchar_set(sw_uart_putc);

    printk("NOTE: HW UART GPIO is in a bad state now\n");

    // do print
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

#undef todo
#define todo(msg)                                      \
    do {                                               \
        emergency_printk("%s:%d:%s\nDONE!!!\n",        \
                         __FUNCTION__, __LINE__, msg); \
        rpi_reboot();                                  \
    } while (0)

// END of the bit bang code.
#endif

//*****************************************************
// the rest you should implement.

enum {
    AUX_BASE = 0x20215000,
    AUX_IRQ = AUX_BASE,
    AUX_ENB = AUX_BASE + 0x4,
    AUX_MU_IO_REG = AUX_BASE + 0x40,
    AUX_MU_IER_REG = AUX_BASE + 0x44,
    AUX_MU_IIR_REG = AUX_BASE + 0x48,
    AUX_MU_LCR_REG = AUX_BASE + 0x4c,
    AUX_MU_CNTL_REG = AUX_BASE + 0x60,
    AUX_MU_STAT_REG = AUX_BASE + 0x64,
    AUX_MU_BAUD = AUX_BASE + 0x68,
};

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    // NOTE: make sure you delete all print calls when
    // done!
    unsigned read;
    unsigned modify;
    // perhaps confusingly: at this point normal printk works
    // since we overrode the system putc routine.
    dev_barrier();
    gpio_set_function(GPIO_TX, GPIO_FUNC_ALT5);
    gpio_set_function(GPIO_RX, GPIO_FUNC_ALT5);
    dev_barrier();

    // Enable uart (p9)
    OR32(AUX_ENB, 0b1);
    dev_barrier();

    // Disable tx,rx, auto flow control (p16,17)
    // Write 0000 to bottom four of AUX_MU_CNTL_REG
    // read = GET32(AUX_MU_CNTL_REG);
    // modify = read >> 2 << 2;
    // PUT32(AUX_MU_CNTL_REG, modify);
    PUT32(AUX_MU_CNTL_REG, 0);

    // Clear the FIFOs (p13)
    // write 1 to bits 1 and 2 of AUX_MU_IIR_REG
    OR32(AUX_MU_IIR_REG, 0b110);

    // Set uart to 8n1 (p14)
    // Write 11 to bottom two bits of AUX_MU_LCR_REG
    OR32(AUX_MU_LCR_REG, 0b11);

    // 115200 baud
    // Write 270 to AUX_MU_BAUD (computed from formula)
    PUT32(AUX_MU_BAUD, 270);

    // Disable interrupts (p12, errata)
    // Write 0 to bottom two bits of AUX_MU_IER_REG
    // read = GET32(AUX_MU_IER_REG);
    // modify = read >> 2 << 2;
    // PUT32(AUX_MU_IER_REG, modify);
    PUT32(AUX_MU_IER_REG, 0);

    // Reenable transmitter and receiver (p17)
    OR32(AUX_MU_CNTL_REG, 0b11);
    dev_barrier();

    // delete everything to do w/ sw-uart when done since
    // it trashes your hardware state and the system
    // <putc>.
    demand(!called_sw_uart_p,
           delete all sw - uart uses or hw UART in bad state);
}

// disable the uart: make sure all bytes have been
//
void uart_disable(void) {
    dev_barrier();
    // Check that all bytes have been transmitted (transmitter is idle)
    // Wait for TX to be idle (p18)
    // Wait until bit 3 of AUX_MU_STAT_REG is 1
    // while (!(GET32(AUX_MU_STAT_REG) && 0b1000)) {
    // }
    uart_flush_tx();

    // Disable tx,rx
    // Write 00 to bottom two of AUX_MU_CNTL_REG
    unsigned read = GET32(AUX_MU_CNTL_REG);
    unsigned modify = read >> 2 << 2;
    PUT32(AUX_MU_CNTL_REG, modify);

    // Disable uart (p9)
    // Write 0 to first bit of AUXENB
    read = GET32(AUX_ENB);
    modify = read >> 1 << 1;
    PUT32(AUX_ENB, modify);
    dev_barrier();
}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is
// at least one byte.
int uart_get8(void) {
    // Wait for there to be a byte in RX (p19)
    // Block until bit 9 of AUX_MU_STAT_REG is 1
    while (!(GET32(AUX_MU_STAT_REG) & (0b1 << 9))) {
    }

    // Pop from receive queue (p11)
    // Bottom 8 bits from AUX_MU_IO_REG
    return (int)(GET32(AUX_MU_IO_REG) & 0b11111111);
}

// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    // Check TX has space (p18)
    // Need bit 1 of AUX_MU_STAT_REG = 1
    return (GET32(AUX_MU_STAT_REG) & 0b10);
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    while (!uart_can_put8()) {
    };
    // Write c to AUX_MU_IO_REG bottom 8 bits (p11)
    PUT32(AUX_MU_IO_REG, c);

    return 1;
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    // Check if bottom bit of AUX_MU_STAT_REG is 1
    return GET32(AUX_MU_STAT_REG) & 0b1;
}

// returns:
//  -1 if no data on the RX FIFO.
//  otherwise reads a byte and returns it.
int uart_get8_async(void) {
    if (!uart_has_data())
        return -1;
    return uart_get8();
}

// returns:
//  - 1 if TX FIFO empty AND idle.
//  - 0 if not empty.
int uart_tx_is_empty(void) {
    // Check if bit 9 of AUX_MU_STAT_REG is set (p18)
    return (GET32(AUX_MU_STAT_REG) & (0b1 << 9));
}

// return only when the TX FIFO is empty AND the
// TX transmitter is idle.
//
// used when rebooting or turning off the UART to
// make sure that any output has been completely
// transmitted.  otherwise can get truncated
// if reboot happens before all bytes have been
// received.
void uart_flush_tx(void) {
    while (!uart_tx_is_empty())
        rpi_wait();
}
