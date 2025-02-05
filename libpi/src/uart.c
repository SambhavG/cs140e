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
#include <stdint.h>

#include "gpio.h"
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

enum {
    GPIO_BASE = 0x20200000,  // base == fsel0
    gpio_set0 = (GPIO_BASE + 0x1C),
    gpio_clr0 = (GPIO_BASE + 0x28),
    gpio_lev0 = (GPIO_BASE + 0x34),

    AUX_BASE = 0x20215000,
    aux_enables = (AUX_BASE + 0x4),
    aux_mu_io_reg = (AUX_BASE + 0x40),
    aux_mu_ier_reg = (AUX_BASE + 0x44),
    aux_mu_iir_reg = (AUX_BASE + 0x48),
    aux_mu_lcr_reg = (AUX_BASE + 0x4C),
    aux_mu_mcr_reg = (AUX_BASE + 0x50),
    aux_mu_lsr_reg = (AUX_BASE + 0x54),
    aux_mu_cntl_reg = (AUX_BASE + 0x60),
    aux_mu_stat_reg = (AUX_BASE + 0x64),
    aux_mu_baud_reg = (AUX_BASE + 0x68)

    // <you may need other values.>
};
//*****************************************************
// the rest you should implement.

// assumes new_value has length n_bits, and lowest_bit_idx is the index of the lowest bit
// eg. it would be 1 if we wanted to write 3-1

// called first to setup uart to 8n1 115200  baud,
// no interrupts.
//  - you will need memory barriers, use <dev_barrier()>
//
//  later: should add an init that takes a baud rate.
void uart_init(void) {
    // NOTE: make sure you delete all print calls when
    // done!
    // emergency_printk("start here\n");

    // perhaps confusingly: at this point normal printk works
    // since we overrode the system putc routine.
    // printk("write UART addresses in order\n");

    // disable uart
    /// rmw(aux_enables, 1, 0, 0);
    // uart_disable();
    // dev_barrier();

    // turn on gpio pins
    dev_barrier();
    // gpio_set_on(14);
    gpio_set_function(GPIO_TX, GPIO_FUNC_ALT5);
    // // gpio_set_on(15);
    gpio_set_function(GPIO_RX, GPIO_FUNC_ALT5);
    // sw_uart = sw_uart_default();

    dev_barrier();

    // enable uart
    PUT32(aux_enables, GET32(aux_enables) | 0b1);

    dev_barrier();
    // disable tx / rx
    PUT32(aux_mu_cntl_reg, 0);
    // clear tx / rx
    // PUT32(aux_mu_iir_reg, 0b11000111);
    PUT32(aux_mu_iir_reg, 0b110);
    // set to 8 bits instead of 7
    PUT32(aux_mu_lcr_reg, 0b11);
    // write to the baud reg
    PUT32(aux_mu_baud_reg, 270);
    // // we don't need this MCR reg, so set it to 0
    // PUT32(aux_mu_mcr_reg, 0);
    // disable interrupts
    PUT32(aux_mu_ier_reg, 0);
    // enable tx / rx
    PUT32(aux_mu_cntl_reg, 0b11);

    dev_barrier();
    // emergency_printk("done initializing\n");

    // delete everything to do w/ sw-uart when done since
    // it trashes your hardware state and the system
    // <putc>.
    // demand(!called_sw_uart_p,
    //        delete all sw - uart uses or hw UART in bad state);
}

// returns:
//  - 1 if TX FIFO empty AND idle.
//  - 0 if not empty.
int uart_tx_is_empty(void) {
    // check if the transmitter is idle
    return GET32(aux_mu_lsr_reg) & (1 << 6);
}
// disable the uart: make sure all bytes have been
//
void uart_disable(void) {
    // wait for transmission to finish and the FIFO queue to be empty
    uart_flush_tx();
    // disable uart
    dev_barrier();
    PUT32(aux_enables, GET32(aux_enables) & ~0b1);
    dev_barrier();
}

// returns:
//  - 1 if at least one byte on the hardware RX FIFO.
//  - 0 otherwise
int uart_has_data(void) {
    return GET32(aux_mu_stat_reg) & 0x1;
}

// returns one byte from the RX (input) hardware
// FIFO.  if FIFO is empty, blocks until there is
// at least one byte.
int uart_get8(void) {
    // while there is nothing to read, block
    while (!uart_has_data()) {
    }
    // return least significant byte
    return GET32(aux_mu_io_reg) & ((0b1 << 9) - 1);
}

// returns 1 if the hardware TX (output) FIFO has room
// for at least one byte.  returns 0 otherwise.
int uart_can_put8(void) {
    return GET32(aux_mu_stat_reg) & 0b10;
}

// put one byte on the TX FIFO, if necessary, waits
// until the FIFO has space.
int uart_put8(uint8_t c) {
    // emergency_printk("start busywaiting in put\n");
    while (!uart_can_put8()) {
    }
    // emergency_printk("done busywaiting in put\n");
    PUT32(aux_mu_io_reg, c);
    return 1;
}

// returns:
//  -1 if no data on the RX FIFO.
//  otherwise reads a byte and returns it.
int uart_get8_async(void) {
    if (!uart_has_data())
        return -1;
    return uart_get8();
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
