#include "rpi.h"
#include "stdint.h"

// Base addresses, from your snippet
#define USB_BASE        0x20980000

// Key Synopsys DWC OTG registers (Global, Host, Power, etc.)
#define GUSBCFG         0x00C
#define GRSTCTL         0x010
#define GINTSTS         0x014
#define GINTMSK         0x018
#define GRXFSIZ         0x024
#define GNPTXFSIZ       0x028
#define HPTXFSIZ        0x100
#define HCFG            0x400
#define HFIR            0x404
#define HFNUM           0x408
#define HPRT            0x440
    #define HPRT_PRTCONNDET      (1 << 1)
    #define HPRT_PRTENA          (1 << 2)
    #define HPRT_PRTENCHNG       (1 << 3)
    #define HPRT_PRTOVRCURRCHNG  (1 << 5)
    #define HPRT_PRTRST          (1 << 8)
    #define HPRT_PRTPWR          (1 << 12)
#define HCCHAR0         0x500
#define HCTSIZ0         0x510
#define HCDMA0          0x514
#define HCINT0          0x508

// Shorthand macros to access registers
#define USB_REG32(reg)  (*(volatile uint32_t*)((USB_BASE) + (reg)))


void usb_host_init(void)
{
    // 1. Put controller into soft reset.
    USB_REG32(GRSTCTL) = (1 << 0); // CoreSoftReset
    // Wait for it to self-clear
    int timeout = 10000;
    while((USB_REG32(GRSTCTL) & 1) && --timeout)
        delay_ms(1);
    if(timeout == 0) {
        printk("ERROR: Core soft reset timed out.\n");
        return;
    }

    // 2. Wait for AHB master IDLE
    timeout = 10000;
    while(((USB_REG32(GRSTCTL) >> 31) & 1) == 0 && --timeout)
        delay_ms(1);
    if(timeout == 0) {
        printk("ERROR: AHB Idle wait timed out.\n");
        return;
    }

    // 3. Configure global USB settings for host mode:
    // GUSBCFG: e.g. set PHY if FS only, or if you have a UTMI+ ...
    // Typical for Full Speed with an internal PHY, you might do:
    uint32_t gusbcfg = USB_REG32(GUSBCFG);
    // For example, force host mode (FDMOD), or if using OTG, you
    // might rely on HNP. But to keep it simple:
    gusbcfg |= (1 << 29);  // ForceHostMode
    // Also ensure "PHY if" is set for FS. 
    // Some bits differ depending on the version of the core or the Pi’s integration
    USB_REG32(GUSBCFG) = gusbcfg;

    // 4. FIFO sizes. You must set these at least once before enabling host mode.
    // Common minimal sizes:  64 or 128 words for Rx FIFO, 64 for NP Tx FIFO, etc.
    USB_REG32(GRXFSIZ)    = 0x80;  // 128 words Rx FIFO
    USB_REG32(GNPTXFSIZ)  = (0x40 << 16) | 0x80; // 64 words for Non-Periodic Tx, start at 128
    // If you plan to do periodic transfers, also set HPTXFSIZ:
    USB_REG32(HPTXFSIZ)   = (0x40 << 16) | (0xC0); // 64 words for Periodic Tx, start at 192

    // 5. Host Configuration: set FS/LS PHY clock selection in HCFG
    // For full speed, FSLSPclkSel=48 MHz/8 => 6 or 48 => 1 ... depends on your integration:
    // Typically for FS, it might be 48MHz 1:1 or 30-60. The Pi’s design is unusual but typically:
    // Let’s assume bit 0..1 = 1 for FS PHY clock (48MHz).
    USB_REG32(HCFG) = (USB_REG32(HCFG) & ~0x3) | 0x1; 

    // 6. Frame Interval. For Full-Speed = 48000 counts per 1ms frame if running a 48MHz PHY clock.
    USB_REG32(HFIR) = 48000;

    // 7. Unmask the global interrupts you need
    USB_REG32(GINTMSK) = 0;
    USB_REG32(GINTSTS) = 0xFFFFFFFF; // clear any pending
    // Typical bits: USB reset detect, port interrupt, channel int, RX FIFO non-empty, ...
    // For simplicity here:
    USB_REG32(GINTMSK) = (1 << 24)  // Host port interrupt
                       | (1 << 25)  // Host channels interrupt
                       | (1 << 4);  // RXFIFO non-empty

    // 8. Switch OTG into host mode (if not done by force host). Some references: GOTGCTL=...
    // If you forced host mode in GUSBCFG above, typically you’re done.

    // 9. Power on the port. (Set PPWR in HPRT)
    uint32_t hprt = USB_REG32(HPRT);
    // Clear bits that are write-clear
    hprt &= ~(HPRT_PRTENA | HPRT_PRTENCHNG | HPRT_PRTCONNDET | HPRT_PRTOVRCURRCHNG);
    // Set power bit
    hprt |= HPRT_PRTPWR;
    USB_REG32(HPRT) = hprt;

    delay_ms(100);
}

//---------------------------------------------------------------------
// Helper to reset the port after connect is detected
//---------------------------------------------------------------------
int usb_port_reset(void)
{
    uint32_t hprt = USB_REG32(HPRT);
    // Clear change bits
    hprt &= ~(HPRT_PRTENA | HPRT_PRTENCHNG | 
              HPRT_PRTCONNDET | HPRT_PRTOVRCURRCHNG);
    // Start reset
    hprt |= HPRT_PRTRST;
    USB_REG32(HPRT) = hprt;

    delay_ms(50);

    // Release reset
    hprt = USB_REG32(HPRT);
    hprt &= ~(HPRT_PRTRST);
    hprt &= ~(HPRT_PRTENA | HPRT_PRTENCHNG | 
              HPRT_PRTCONNDET | HPRT_PRTOVRCURRCHNG);
    USB_REG32(HPRT) = hprt;

    delay_ms(50);

    // Check if still connected
    hprt = USB_REG32(HPRT);
    if((hprt & (1 << 1)) == 0) {
        printk("Device disconnected during reset.\n");
        return -1;
    }

    // Return the port speed: (bits 17:16)
    return (hprt >> 17) & 0x3; 
}

//---------------------------------------------------------------------
// Example function to do a simple control transfer (GET_DESCRIPTOR)
// on endpoint 0, from device at address 'dev_addr'.
//
// Returns length actually read into 'buf' or negative on error.
//---------------------------------------------------------------------
int usb_control_in(uint8_t dev_addr, uint8_t request_type,
                   uint8_t request, uint16_t value, uint16_t index,
                   uint8_t *buf, uint16_t length)
{
    // For brevity, we show just one attempt. Real code should
    // handle re-tries, toggles, etc.

    // 1) SETUP stage (8 bytes)
    // You can store the 8 bytes in a buffer aligned to 4, for DMA
    static uint32_t setup_data[2] __attribute__((aligned(4)));
    setup_data[0] = (request_type) 
                  | (request << 8) 
                  | (value << 16);          // lower 16 bits of wValue
    setup_data[1] = ((value >> 16) & 0xffff) // upper bits if needed, typically 0
                  | (index << 16);
                  // | (length << 32);         // Not possible in 32 bits => we do manual packing

    // Actually, for the standard GET_DESCRIPTOR:
    // wValue = (descriptor_type << 8) | descriptor_index
    // e.g. GET_DESCRIPTOR(Device=0x01) => value= 0x0100
    // So you might encode it carefully. But let's keep it conceptual.

    // The DWC driver code typically packs them as two 32-bit words, so:
    // Word0: bmRequestType, bRequest, wValue(LSB, MSB)
    // Word1: wIndex(LSB, MSB), wLength(LSB, MSB)

    // 2) Configure channel 0: 
    // Make sure it's halted first:
    uint32_t hcchar = USB_REG32(HCCHAR0);
    if(!(hcchar & (1<<31))) {
        printk("Halt was not set\n");
        hcchar |= (1 << 31);
        USB_REG32(HCCHAR0) = hcchar;
        // wait for halt to be set...
        int to = 1000;
        while(to-- && !(USB_REG32(HCCHAR0) & (1<<31))) {
            delay_ms(1);
        }
        USB_REG32(HCINT0) = 0xFFFF; // clear any leftover
        printk("Halt has been set set\n");
    }

    // (a) SETUP Stage: 8 bytes out
    //   - HCTSIZ0: PKTCNT=1, XFRSIZ=8, PID=Setup=3
    //   - HCCHAR0: MPS=64, EpNum=0, DevAddr=dev_addr, EPNDir=OUT=0, Channel enable
    USB_REG32(HCDMA0)  = (uint32_t)setup_data;
    USB_REG32(HCTSIZ0) = ( (8 << 0)      // XFRSIZ=8
                         | (1 << 19)     // PKTCNT=1
                         | (3 << 29));   // PID=SETUP (binary 11)
    hcchar = (64 << 0)                // MPS=64
           | (0 << 11)               // EpNum=0
           | ((dev_addr & 0x7F) << 15)   // Device Address
           | (1 << 29)               // Channel enable
           | (0 << 30);              // OUT
    USB_REG32(HCCHAR0) = hcchar;

    // Wait for completion
    int timeout = 1000;
    while(timeout--) {
        uint32_t hcint = USB_REG32(HCINT0);
        if(hcint & 0x01) {
            // Xfer Complete
            USB_REG32(HCINT0) = 0xFFFF; // Clear
            break;
        }
        delay_ms(1);
    }
    if(timeout <= 0) {
        printk("Timeout waiting for SETUP stage.\n");
        return -1;
    }

    // (b) DATA Stage: IN
    if(length > 0) {
        // Prepare your buffer for DMA IN
        USB_REG32(HCDMA0) = (uint32_t)buf;
        // PKTCNT = ceil(length / MPS=64)
        int pktcnt = (length + 63) / 64; 
        // Set PID=DATA1 => 2 in DWC (for first data stage after SETUP)
        USB_REG32(HCTSIZ0) = ( (length << 0)
                             | (pktcnt << 19)
                             | (2 << 29)); // DATA1

        hcchar = (64 << 0)
               | (0 << 11) 
               | ((dev_addr & 0x7F) << 15) 
               | (1 << 29)  // Enable
               | (1 << 30); // IN
        USB_REG32(HCCHAR0) = hcchar;

        // Wait
        timeout = 1000;
        while(timeout--) {
            uint32_t hcint = USB_REG32(HCINT0);
            if(hcint & 0x01) {
                // Transfer Complete
                USB_REG32(HCINT0) = 0xFFFF;
                break;
            }
            delay_ms(1);
        }
        if(timeout <= 0) {
            printk("Timeout waiting for DATA IN stage.\n");
            return -1;
        }
    }

    // (c) STATUS Stage: OUT (ZLP)
    USB_REG32(HCDMA0) = 0; // no data
    USB_REG32(HCTSIZ0) = ( (0 << 0)      // XFRSIZ=0
                         | (1 << 19)     // PKTCNT=1
                         | (1 << 29));   // PID=DATA1 for STATUS stage
    hcchar = (64 << 0)
           | (0 << 11)
           | ((dev_addr & 0x7F) << 15)
           | (1 << 29)  // enable
           | (0 << 30); // OUT
    USB_REG32(HCCHAR0) = hcchar;

    timeout = 1000;
    while(timeout--) {
        uint32_t hcint = USB_REG32(HCINT0);
        if(hcint & 0x01) {
            USB_REG32(HCINT0) = 0xFFFF;
            break;
        }
        delay_ms(1);
    }
    if(timeout <= 0) {
        printk("Timeout waiting for STATUS stage.\n");
        return -1;
    }

    // Return the # of bytes read
    return length;
}

//---------------------------------------------------------------------
// MAIN enumeration example
//---------------------------------------------------------------------
void notmain(void)
{
    printk("Initializing USB in Host mode...\n");
    usb_host_init();

    // Wait to see if we detect a device
    // Poll HPRT for "Connect" bit
    int tries = 50;
    while(tries--) {
        uint32_t hprt = USB_REG32(HPRT);
        if(hprt & (1 << 1)) {
            printk("Device connect detected!\n");
            break;
        }
        delay_ms(100);
    }
    if(tries <= 0) {
        printk("No device detected, exiting.\n");
        return;
    }

    // Reset the port
    int speed = usb_port_reset();
    if(speed < 0) return;
    printk("Port speed = %d (0=High,1=Full,2=Low,...)\n", speed);

    // By default, device address=0 at this point
    // Let's GET_DESCRIPTOR(Device) from address=0
    static uint8_t dev_desc[64] __attribute__((aligned(4)));
    int rc = usb_control_in(0, 0x80, /*GET_DESCRIPTOR*/0x06, 
                            /*wValue=(DEVICE<<8)|0*/ (0x01 << 8), 
                            0, dev_desc, 18);
    if(rc < 0) {
        printk("Failed to get device descriptor.\n");
        return;
    }
    printk("Got Device Descriptor (first 18 bytes):\n");
    for(int i=0; i<18; i++) {
        printk(" %02X", dev_desc[i]);
    }
    printk("\n");

    uint16_t idVendor  = dev_desc[8] | (dev_desc[9]<<8);
    uint16_t idProduct = dev_desc[10] | (dev_desc[11]<<8);

    printk("Vendor: %04X, Product: %04X\n", idVendor, idProduct);

    // Next: assign a new address (1..127). We'll choose 1
    // SET_ADDRESS
    rc = usb_control_in(0, 0x00, /*SET_ADDRESS*/0x05,
                        /*wValue=new address*/1, 0, NULL, 0);
    if(rc<0) {
        printk("Failed to SET_ADDRESS\n");
        return;
    }
    delay_ms(10); // spec requires small delay
    // Now subsequent requests must use 'dev_addr=1'

    // (Re‐read the device descriptor at address=1 to confirm)
    rc = usb_control_in(1, 0x80, 0x06, (0x01<<8), 0, dev_desc, 18);
    if(rc<0) {
        printk("Failed again to read device descriptor.\n");
        return;
    }

    // Now check if this is your thermal printer
    if(idVendor == 0x0416 && idProduct == 0x5011) {
        printk("Thermal printer detected!\n");
        // Proceed to read configuration, set configuration, etc.
        // Then you’d locate the Bulk OUT endpoint and send data.
    } else {
        printk("Some other device, not the 0x0416:0x5011 printer.\n");
    }
    
    // ...
    // You would continue full enumeration: GET_DESCRIPTOR(Config), parse endpoints,
    // SET_CONFIGURATION, etc. Then do a Bulk Out to send print data.

    printk("Done.\n");
}
