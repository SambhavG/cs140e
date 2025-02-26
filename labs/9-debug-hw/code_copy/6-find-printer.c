#include "stdint.h"
#include "rpi.h"

#define USB_BASE        0x7E980000

// Important USB register offsets
#define GOTGCTL         0x000   // OTG Control and Status
#define GRSTCTL        0x010   // Reset Control
#define GINTSTS        0x014   // Core Interrupt Status
#define HPRT           0x440   
#define HPRT           0x440   // Host Port Control & Status
#define HPRT_PRTPWR    (1<<12) // Port Power bit

// Additional register definitions
#define HPRT_PRTRST    (1<<8)  // Port Reset bit
#define HPRT_PRTENA    (1<<2)  // Port Enable bit
#define HFNUM          0x408   // Frame Number register
#define HCCHAR0        0x500   // Host Channel 0 Characteristics
#define HCINT0         0x508   // Host Channel 0 Interrupt
#define GRXSTSR        0x01C   // Receive Status Debug Read
#define GRXSTSP        0x020   // Receive Status Read & POP

// Additional USB definitions
#define HCTSIZ0         0x510   // Host Channel 0 Transfer Size
#define HCDMA0          0x514   // Host Channel 0 DMA Address

// USB request types
#define USB_DIR_OUT     0x00
#define USB_DIR_IN      0x80
#define USB_REQ_GET_DESC 0x06
#define USB_DT_DEVICE   0x01

// HPRT register bit definitions
#define HPRT_PRTCONNDET      (1<<1)  // Port Connect Detected
#define HPRT_PRTENA          (1<<2)  // Port Enable
#define HPRT_PRTENCHNG       (1<<3)  // Port Enable/Disable Change
#define HPRT_PRTOVRCURRCHNG  (1<<5)  // Port Overcurrent Change
#define HPRT_PRTRST          (1<<8)  // Port Reset
#define HPRT_PRTPWR          (1<<12) // Port Power

// Add these additional register definitions at the top
#define GUSBCFG         0x00C   // Global USB Configuration
#define GRXFSIZ         0x024   // RX FIFO Size
#define GNPTXFSIZ       0x028   // Non-Periodic TX FIFO Size
#define HPTXFSIZ        0x100   // Host Periodic TX FIFO Size
#define HCFG            0x400   // Host Configuration
#define HFIR            0x404   // Host Frame Interval

void notmain(void) {
    uint32_t *usb_base = (uint32_t*)(USB_BASE - 0x7E000000 + 0x20000000);
    
    printk("Initializing USB...\n");
    
    // 1. Core soft reset
    uint32_t grstctl = (1 << 0);  // Core soft reset
    usb_base[GRSTCTL/4] = grstctl;
    
    // Wait for reset to complete
    int timeout = 1000;
    while(timeout--) {
        if(!(usb_base[GRSTCTL/4] & (1 << 0))) {
            break;
        }
        delay_ms(1);
    }
    if(timeout <= 0) {
        printk("ERROR: Timeout waiting for core reset\n");
        return;
    }

    // 2. Wait for AHB master IDLE
    timeout = 1000;
    while(timeout--) {
        if(usb_base[GRSTCTL/4] & (1 << 31)) {
            break;
        }
        delay_ms(1);
    }
    if(timeout <= 0) {
        printk("ERROR: Timeout waiting for AHB master idle\n");
        return;
    }

    // 3. Configure global USB settings for host mode
    uint32_t gusbcfg = usb_base[GUSBCFG/4];
    gusbcfg |= (1 << 29);  // ForceHostMode
    usb_base[GUSBCFG/4] = gusbcfg;

    // 4. Configure FIFO sizes
    usb_base[GRXFSIZ/4] = 0x80;  // 128 words RX FIFO
    usb_base[GNPTXFSIZ/4] = (0x40 << 16) | 0x80;  // 64 words Non-Periodic TX FIFO
    usb_base[HPTXFSIZ/4] = (0x40 << 16) | 0xC0;   // 64 words Periodic TX FIFO

    // 5. Host Configuration - set FS PHY clock
    usb_base[HCFG/4] = (usb_base[HCFG/4] & ~0x3) | 0x1;

    // 6. Frame Interval for FS
    usb_base[HFIR/4] = 48000;  // 48MHz clock

    // 7. Enable interrupts
    usb_base[0x018/4] = 0;  // Clear all interrupt masks first
    usb_base[GINTSTS/4] = 0xFFFFFFFF;  // Clear any pending interrupts
    uint32_t gintmsk = (1 << 24) |    // Host port interrupt
                       (1 << 25) |    // Host channels interrupt
                       (1 << 4);      // Receive FIFO non-empty
    usb_base[0x018/4] = gintmsk;

    // 8. Power on the port
    uint32_t hprt = usb_base[HPRT/4];
    hprt &= ~(HPRT_PRTENA | HPRT_PRTENCHNG | HPRT_PRTCONNDET | HPRT_PRTOVRCURRCHNG);
    hprt |= HPRT_PRTPWR;
    usb_base[HPRT/4] = hprt;
    
    delay_ms(100);  // Wait for power to stabilize
    
    // Read initial port status
    hprt = usb_base[HPRT/4];
    printk("Initial Port Status: %x\n", hprt);
    
    if(hprt & HPRT_PRTCONNDET) {  // If device connected
        printk("Device detected, starting reset sequence...\n");
        
        // Start port reset - be careful with register
        hprt = usb_base[HPRT/4];
        hprt &= ~(HPRT_PRTENA | HPRT_PRTCONNDET | HPRT_PRTENCHNG | HPRT_PRTOVRCURRCHNG);
        hprt |= HPRT_PRTRST;
        usb_base[HPRT/4] = hprt;
        
        delay_ms(50);  // Hold reset for 50ms
        
        // Release reset
        hprt = usb_base[HPRT/4];
        hprt &= ~(HPRT_PRTENA | HPRT_PRTCONNDET | HPRT_PRTENCHNG | HPRT_PRTOVRCURRCHNG);
        hprt &= ~HPRT_PRTRST;
        usb_base[HPRT/4] = hprt;
        
        delay_ms(20);  // Wait for device to recover

        // Check if device is still connected
        hprt = usb_base[HPRT/4];
        if(!(hprt & HPRT_PRTCONNDET)) {
            printk("ERROR: Device disconnected during reset!\n");
            return;
        }
        printk("HPRT: %b\n", hprt);
        printk("Post-reset Port Status: %x\n", hprt);
        printk("Port connected: %d\n", (hprt >> 1) & 1);
        printk("Port enabled: %d\n", (hprt >> 2) & 1);
        printk("Port speed: %d\n", (hprt >> 17) & 3);
        
        // Wait for port to be stable
        delay_ms(100);
        clean_reboot();


        // After reset, determine max packet size based on port speed
        hprt = usb_base[HPRT/4];
        uint32_t port_speed = (hprt >> 17) & 3;
        uint32_t max_packet = (port_speed == 2) ? 8 : 64;  // 8 for full-speed, 64 for high-speed
        
        printk("Port Speed: %d, Using max packet size: %d\n", port_speed, max_packet);

        // Before starting transfers, make sure channel 0 is halted
        uint32_t hcchar0 = usb_base[HCCHAR0/4];
        if(!(hcchar0 & (1 << 31))) {  // If not already halted
            hcchar0 |= (1 << 31);      // Set halt bit
            usb_base[HCCHAR0/4] = hcchar0;
            
            // Wait for halt to complete
            timeout = 1000;
            while(timeout--) {
                if(usb_base[HCCHAR0/4] & (1 << 31)) {
                    break;
                }
                delay_ms(1);
            }
            if(timeout <= 0) {
                printk("ERROR: Timeout halting channel 0\n");
                return;
            }
        }

        // Clear any channel interrupts
        usb_base[HCINT0/4] = 0xFFFF;

        // Configure Channel 0 for SETUP stage
        uint32_t hcchar = (max_packet << 0) |  // Maximum packet size
                         (0 << 11)          |  // Endpoint 0
                         (0 << 15)          |  // Device address 0
                         (1 << 29)          |  // Channel enable
                         (0 << 30);            // Direction OUT

        // Program HCTSIZ0 for SETUP transfer
        uint32_t hctsiz = (8 << 0)     |  // Transfer size (8 bytes for setup)
                         (1 << 19)      |  // Packet count (1)
                         (3 << 29);        // Setup stage

        // Prepare setup packet with correct byte ordering
        uint32_t setup_data[2] = {
            USB_DIR_IN | (USB_REQ_GET_DESC << 8) | (0 << 16) | (USB_DT_DEVICE << 24),
            (0 << 0) | (18 << 16)  // wIndex = 0, wLength = 18
        };

        // Allocate buffer for descriptor
        uint8_t descriptor[18] __attribute__((aligned(4))) = {0};
        
        // Write DMA address and start SETUP stage
        usb_base[HCDMA0/4] = (uint32_t)setup_data;
        usb_base[HCTSIZ0/4] = hctsiz;
        usb_base[HCCHAR0/4] = hcchar;

        // Wait for SETUP completion
        timeout = 10000;
        while(timeout--) {
            uint32_t intsts = usb_base[HCINT0/4];
            printk("Setup stage intsts: %x\n", intsts);
            if(intsts & 0x1) {  // Transfer completed
                usb_base[HCINT0/4] = 0xFFFF;  // Clear interrupts
                printk("Setup stage complete\n");
                break;
            }
            delay_ms(1);
        }
        if(timeout <= 0) {
            printk("ERROR: Timeout in SETUP stage\n");
            return;
        }

        // Configure for DATA IN stage
        hcchar = (max_packet << 0) |  // Maximum packet size
                 (0 << 11)         |  // Endpoint 0
                 (0 << 15)         |  // Device address 0
                 (1 << 29)         |  // Channel enable
                 (1 << 30);           // Direction IN

        hctsiz = (18 << 0)    |  // Transfer size (18 bytes for device descriptor)
                 (3 << 19)    |  // Packet count (ceil(18/max_packet))
                 (2 << 29);      // Data stage

        usb_base[HCDMA0/4] = (uint32_t)descriptor;
        usb_base[HCTSIZ0/4] = hctsiz;
        usb_base[HCCHAR0/4] = hcchar;

        // Wait for DATA completion
        timeout = 1000;
        while(timeout--) {
            uint32_t intsts = usb_base[HCINT0/4];
            if(intsts & 0x1) {  // Transfer completed
                usb_base[HCINT0/4] = 0xFFFF;  // Clear interrupts
                printk("Data stage complete\n");
                
                // Print device information
                printk("Device Descriptor:\n");
                printk("  bLength: %d\n", descriptor[0]);
                printk("  bDescriptorType: %d\n", descriptor[1]);
                printk("  bcdUSB: %x.%x\n", descriptor[3], descriptor[2]);
                printk("  bDeviceClass: %x\n", descriptor[4]);
                printk("  bDeviceSubClass: %x\n", descriptor[5]);
                printk("  bDeviceProtocol: %x\n", descriptor[6]);
                printk("  idVendor: %x%x\n", descriptor[9], descriptor[8]);
                printk("  idProduct: %x%x\n", descriptor[11], descriptor[10]);
                
                // If it's a printer class device (0x07)
                if(descriptor[4] == 0x07) {
                    printk("Confirmed USB Printer Device!\n");
                }
                break;
            }
            delay_ms(1);
        }
        if(timeout <= 0) {
            printk("ERROR: Timeout in DATA stage\n");
            return;
        }

        // STATUS stage (OUT, zero-length packet)
        hcchar = (max_packet << 0) |  // Maximum packet size
                 (0 << 11)         |  // Endpoint 0
                 (0 << 15)         |  // Device address 0
                 (1 << 29)         |  // Channel enable
                 (0 << 30);           // Direction OUT

        hctsiz = (0 << 0)     |  // Zero-length transfer
                 (1 << 19)    |  // Packet count = 1
                 (1 << 29);      // Status stage

        usb_base[HCDMA0/4] = 0;  // No data buffer needed
        usb_base[HCTSIZ0/4] = hctsiz;
        usb_base[HCCHAR0/4] = hcchar;

        // Wait for STATUS completion
        timeout = 1000;
        while(timeout--) {
            uint32_t intsts = usb_base[HCINT0/4];
            if(intsts & 0x1) {  // Transfer completed
                usb_base[HCINT0/4] = 0xFFFF;  // Clear interrupts
                printk("Status stage complete\n");
                break;
            }
            delay_ms(1);
        }
        if(timeout <= 0) {
            printk("ERROR: Timeout in STATUS stage\n");
            return;
        }

    } else {
        printk("No device detected!\n");
    }
}
