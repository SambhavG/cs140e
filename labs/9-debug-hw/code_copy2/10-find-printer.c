/*
 * Bare-metal Raspberry Pi Zero example to send ESC/POS commands
 * via the MDIO interface to a USB thermal printer.
 *
 * According to the Broadcom documentation:
 *  - Before each MDIO access, write 0xFFFFFFFF (preamble).
 *  - Write the 32-bit command/data word.
 *  - Write 0x00000000 afterward (extra clock edge for a PHY bug).
 *
 * This example sends the ESC @ command (0x1B 0x40) to initialize the printer,
 * followed by the text "Hello, world!" and a newline.
 */

#include <stdint.h>

// Memory-mapped register addresses for the USB MDIO interface.
#define USB_MDIO_CNTL  (*(volatile uint32_t *)0x7E980080)
#define USB_MDIO_GEN   (*(volatile uint32_t *)0x7E980084)
// (USB_VBUS_DRV, etc., can be used for additional control if required.)

// Perform one MDIO transfer (of one 32-bit word).
void mdio_transfer(uint32_t data) {
    // Wait until the MDIO interface is idle (bit 31 of USB_MDIO_CNTL == 0).
    while (USB_MDIO_CNTL & (1U << 31)) { }
    
    // Write the preamble.
    USB_MDIO_GEN = 0xFFFFFFFF;
    
    // Write the actual 32-bit data word.
    USB_MDIO_GEN = data;
    
    // Write a trailing word to generate an extra clock edge.
    USB_MDIO_GEN = 0x00000000;
}

// Send an array of bytes over MDIO. This function packs bytes
// into 32-bit words (little-endian) and sends each word.
void mdio_send_bytes(const uint8_t *data, int length) {
    int i = 0;
    while (i < length) {
        uint32_t word = 0;
        // Pack up to 4 bytes into one 32-bit word.
        for (int j = 0; j < 4 && i < length; j++, i++) {
            word |= ((uint32_t)data[i]) << (8 * j);
        }
        mdio_transfer(word);
    }
}

// ESC/POS commands and text to be sent to the printer.
const uint8_t esc_init[] = { 0x1B, 0x40 };   // ESC @: Initialize printer.
const uint8_t hello_text[] = "Hello, world!\n";

// Entry point for bare-metal code.
void notmain(void) {
    // Optional: Initialize additional USB or board settings if needed.
    
    // Send the printer initialization command.
    mdio_send_bytes(esc_init, sizeof(esc_init));
    
    // Send the text to print.
    mdio_send_bytes(hello_text, sizeof(hello_text) - 1); // Exclude the null terminator.
    
    
}
