#ifndef DWC_REGS_H
#define DWC_REGS_H

#include <stdint.h>
#include <rpi.h>

/* Base address for USB controller on Raspberry Pi Zero (BCM2835).
   If needed, adjust to match your mapping: 0x20980000, 0x7E980000, etc. */
#define USB_BASE 0x20980000

/* Global registers offset */
#define DWC_GAHBCFG          0x008
#define DWC_GUSBCFG          0x00C
#define DWC_GINTSTS          0x014
#define DWC_GINTMSK          0x018

/* Host mode register offsets */
#define DWC_HCFG             0x400
#define DWC_HPRT             0x440
#define DWC_HAINT            0x414
#define DWC_HAINTMSK         0x418

/* Host channel registers: each channel has a set of registers */
#define DWC_HCCHAR(ch)       (0x500 + (ch)*0x20)
#define DWC_HCINT(ch)        (0x508 + (ch)*0x20)
#define DWC_HCINTMSK(ch)     (0x50C + (ch)*0x20)
#define DWC_HCTSIZ(ch)       (0x510 + (ch)*0x20)
#define DWC_HCDMA(ch)        (0x514 + (ch)*0x20)

/* A convenience macro to access 32-bit registers */
static inline void mmio_write(uint32_t reg, uint32_t val)
{
    *(volatile uint32_t *)(USB_BASE + reg) = val;
}

static inline uint32_t mmio_read(uint32_t reg)
{
    return *(volatile uint32_t *)(USB_BASE + reg);
}

#endif /* DWC_REGS_H */


#include <stdint.h>

#define DELAY_COUNT 0x100000

/* Simple function to introduce a delay */
static void delay(unsigned int count)
{
    while(count--) {
        __asm__ volatile ("nop");
    }
}

/* ---------------------- Low-level DWC Helper Routines ---------------------- */

/* Wait for device connection (polling HPRT) */
static void wait_for_device_connection(void)
{
    while(1)
    {
        uint32_t hprt = mmio_read(DWC_HPRT);
        if (hprt & (1 << 1)) /* Check "Device Connected" bit */
        {
            /* Some hardware might need extra debouncing or reset. */
            break;
        }
    }
}

/* Power on and reset the port to initiate a USB reset on the device */
static void reset_port(void)
{
    /* Power on the port */
    uint32_t hprt = mmio_read(DWC_HPRT);
    hprt |= (1 << 12); // PRTPWR=1
    mmio_write(DWC_HPRT, hprt);

    /* Wait a bit for power to stabilize */
    delay(0x100000);

    /* Now do a USB reset */
    hprt = mmio_read(DWC_HPRT);
    /* Clear any bits that we might re-set */
    hprt &= ~((1 << 8) | (1 << 3) | (1 << 2)); /* Clear PRTRST, etc. */
    /* Set Port Reset bit */
    hprt |= (1 << 8);
    mmio_write(DWC_HPRT, hprt);

    /* Wait for 50+ ms (USB spec) */
    delay(0x200000);

    /* Clear the reset bit */
    hprt = mmio_read(DWC_HPRT);
    hprt &= ~(1 << 8);
    mmio_write(DWC_HPRT, hprt);
}

/* Very naive poll for a channel's XferCompl or Halt or error. */
static int wait_for_channel_complete(uint32_t channel)
{
    while (1)
    {
        uint32_t hcint = mmio_read(DWC_HCINT(channel));
        if (hcint & (1 << 2))  /* Stall */
        {
            /* Clear the interrupt bits by writing them back */
            mmio_write(DWC_HCINT(channel), 0x7FF);
            return -1; // STALL
        }
        if (hcint & (1 << 3))  /* Nak */
        {
            mmio_write(DWC_HCINT(channel), 0x7FF);
            return -2; // NAK
        }
        if (hcint & (1 << 0))  /* XferCompl */
        {
            mmio_write(DWC_HCINT(channel), 0x7FF);
            return 0;  // success
        }
        // NOTE: more error bits could be handled
    }
}

/* Set up a host channel for a control-OUT or control-IN transaction with EP0 */
static int do_control_transfer(uint8_t device_address,
                               uint8_t *setup_data, int setup_len,
                               uint8_t *data_buf,   int data_len,
                               int is_in)
{
    /* For simplicity, use Host Channel 0 for SETUP and data stages. */

    /* Clear old interrupts on channel 0 */
    mmio_write(DWC_HCINT(0), 0x7FF);  /* Clear all possible interrupts */

    /* 1) SETUP stage: 8 bytes typically */
    {
        /* Program HCTSIZ for one packet of 'setup_len' (usually 8) */
        mmio_write(DWC_HCTSIZ(0),
            ((1 << 19) |               /* Packet count = 1 */
             (setup_len & 0x7FFFF) |   /* transfer size */
             (3 << 29))                /* PID=SETUP (b'11) for Setup stage */
        );

        /* Program HCDMA with the pointer to the setup data */
        mmio_write(DWC_HCDMA(0), (uint32_t)(uintptr_t)setup_data);

        /* Program HCCHAR:
         * LSDEV=0 (assuming full-speed device),
         * EP0, EP dir=0(OUT) for setup stage,
         * EPTYPE=0 (control),
         * MPS=8 (assuming device’s EP0 max packet size=8),
         * DevAddr=device_address,
         * CHENA=1 to enable
         */
        uint32_t hcchar_val = (8 << 0) |                /* MPS = 8 */
                              ((uint32_t)device_address << 22) | /* DevAddr in bits [29:22]? 
                                                                   * Actually, in DWC2 it might be [31:22],
                                                                   * adjust if needed. 
                                                                   */
                              (0 << 11) |               /* Endpoint #0 */
                              (0 << 15) |               /* EPDIR=0 => OUT */
                              (0 << 18) |               /* EPTYPE=0 => CONTROL */
                              (1 << 20) |               /* LSDEV=0 => FS (some bits differ) */
                              (1 << 31);                /* CHENA=1 (enable) 
                                                         * In some DWC versions, CHENA is bit 31,
                                                         * confirm in your docs.
                                                         */
        mmio_write(DWC_HCCHAR(0), hcchar_val);

        /* Wait for transfer complete */
        if (wait_for_channel_complete(0) != 0)
            return -1; // error (NAK, stall, etc.)
    }

    /* If there's a Data stage, do it next. */
    if (data_len > 0)
    {
        /* For simplicity, assume only one packet (<= 8 or <=64).
           Real code should handle multiple packets if data_len > MPS. */
        mmio_write(DWC_HCINT(0), 0x7FF);  /* Clear old interrupts */

        mmio_write(DWC_HCTSIZ(0),
            ((1 << 19) |               /* Packet count = 1 */
             (data_len & 0x7FFFF) |
             ((is_in ? 2 : 1) << 29))  /* PID=DATA1 for 1st data stage in control: 
                                        * For IN => b'10, for OUT => b'01. 
                                        */
        );
        mmio_write(DWC_HCDMA(0), (uint32_t)(uintptr_t)data_buf);

        /* For the data stage, direction bit in HCCHAR = is_in?1:0. */
        uint32_t hcchar_val = (8 << 0) |                /* MPS = 8 (assuming device’s EP0) */
                              ((uint32_t)device_address << 22) |
                              (0 << 11) |               /* Endpoint #0 */
                              ((is_in ? 1 : 0) << 15) | /* EPDIR = 1 if IN, else 0 if OUT */
                              (0 << 18) |               /* EPTYPE=0 => CONTROL */
                              (1 << 20) |               /* LSDEV=0 => FS */
                              (1 << 31);                /* CHENA=1 */
        mmio_write(DWC_HCCHAR(0), hcchar_val);

        if (wait_for_channel_complete(0) != 0)
            return -2; // error
    }

    /* STATUS stage (reverse direction of data stage) */
    {
        mmio_write(DWC_HCINT(0), 0x7FF);  /* Clear old interrupts */

        mmio_write(DWC_HCTSIZ(0),
            ((1 << 19) |   /* Packet count = 1 */
             (0 & 0x7FFFF) |
             ((is_in ? 1 : 2) << 29)) /* If data was IN, status is OUT => PID=DATA1(b'01).
                                       * If data was OUT, status is IN => PID=DATA2(b'10)? 
                                       * Actually for control, typically use DATA1 for status too. 
                                       * The exact DWC usage may differ slightly. 
                                       */
        );
        /* No actual data buffer for status stage if it's an OUT, but set a valid address anyway. */
        mmio_write(DWC_HCDMA(0), (uint32_t)(uintptr_t)data_buf);

        /* For status stage, direction is opposite of data stage. */
        uint32_t hcchar_val = (8 << 0) |
                              ((uint32_t)device_address << 22) |
                              (0 << 11) | 
                              ((is_in ? 0 : 1) << 15) | /* flip direction */
                              (0 << 18) |
                              (1 << 20) |
                              (1 << 31);

        mmio_write(DWC_HCCHAR(0), hcchar_val);

        if (wait_for_channel_complete(0) != 0)
            return -3; // error
    }

    return 0; // success
}

/* Helper for standard control requests (SETUP packet) */
struct usb_setup_packet {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed));

static int usb_control_request(uint8_t device_addr,
                               uint8_t request_type, uint8_t request,
                               uint16_t value, uint16_t index,
                               uint16_t length,
                               uint8_t *buffer)
{
    struct usb_setup_packet setup;
    setup.bmRequestType = request_type;
    setup.bRequest      = request;
    setup.wValue        = value;
    setup.wIndex        = index;
    setup.wLength       = length;

    /* For Control in vs out: bit7 of bmRequestType = direction. */
    int is_in = (request_type & 0x80) ? 1 : 0;

    return do_control_transfer(device_addr,
                               (uint8_t*)&setup, sizeof(setup),
                               buffer, length,
                               is_in);
}

/* Minimal GET_DESCRIPTOR(DEVICE or CONFIG) example. */
static int get_descriptor(uint8_t device_addr,
                          uint8_t desc_type, uint8_t desc_index,
                          uint8_t *buf, uint16_t length)
{
    /* bmRequestType: 0x80 => Device-to-host standard request
       bRequest: 0x06 => GET_DESCRIPTOR
       wValue: high-byte = descriptor type, low-byte = descriptor index
       wIndex: 0
       wLength: length
    */
    return usb_control_request(device_addr,
        0x80,  /* device-to-host, standard, device */
        0x06,  /* GET_DESCRIPTOR */
        ((uint16_t)desc_type << 8) | desc_index,
        0,
        length,
        buf);
}

static int set_address(uint8_t old_addr, uint8_t new_addr)
{
    /* bmRequestType=0x00 => host-to-device, standard, device
       bRequest=0x05 => SET_ADDRESS
       wValue=new_addr
       wIndex=0
       wLength=0
    */
    int rc = usb_control_request(old_addr,
        0x00,  /* host-to-device, standard, device */
        0x05,  /* SET_ADDRESS */
        new_addr,
        0,
        0,
        NULL);
    /* Spec says: after receiving status stage of this request, device
       won't actually use new address until next setup token. So we
       might delay a bit or do next control request with new_addr. */
    delay(0x10000);
    return rc;
}

static int set_configuration(uint8_t device_addr, uint8_t config_value)
{
    /* bmRequestType=0x00 => host-to-device, standard, device
       bRequest=0x09 => SET_CONFIGURATION
       wValue=config_value
       wIndex=0
       wLength=0
    */
    return usb_control_request(device_addr,
        0x00,
        0x09,
        config_value,
        0,
        0,
        NULL);
}

/* ---------------------- Minimal Host Init ---------------------- */
static void usb_host_init(void)
{
    // 1) Put core in host mode
    uint32_t gusbcfg = mmio_read(DWC_GUSBCFG);
    gusbcfg |= (1 << 29);  // ForceHostMode
    gusbcfg &= ~(1 << 30); // UnforceDevMode if set
    mmio_write(DWC_GUSBCFG, gusbcfg);

    delay(DELAY_COUNT);

    // 2) Enable AHB master
    uint32_t gahbcfg = mmio_read(DWC_GAHBCFG);
    gahbcfg |= (1 << 0); // Global int enable
    mmio_write(DWC_GAHBCFG, gahbcfg);

    // 3) Configure host parameters: e.g. HCFG for full-speed
    //    Full-speed PHY clock = 48 MHz => HCFG=1 for 48 MHz
    uint32_t hcfg = mmio_read(DWC_HCFG);
    hcfg &= ~0x3;
    hcfg |= 0x1; // For 48MHz full-speed PHY
    mmio_write(DWC_HCFG, hcfg);
}

/* ---------------------- Descriptor Parsing Helpers ---------------------- */

struct usb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed));

struct usb_configuration_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed));

struct usb_interface_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed));

struct usb_endpoint_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed));

static int find_printer_bulk_out_endpoint(const uint8_t *config_buf, int config_len,
                                          uint8_t *out_endpoint, uint8_t *config_value)
{
    /* This function does a very naive parse of the config descriptor buffer:
       - finds an Interface with bInterfaceClass=0x07 (Printer class)
       - reads endpoints to find a Bulk OUT endpoint.
       - returns endpoint number in *out_endpoint
       - returns the bConfigurationValue in *config_value
    */
    if (config_len < sizeof(struct usb_configuration_descriptor))
        return -1;

    /* The very first bytes in config_buf should be configuration descriptor */
    const struct usb_configuration_descriptor *cfg =
        (const struct usb_configuration_descriptor *)config_buf;

    *config_value = cfg->bConfigurationValue; /* store the config ID */

    /* We need to iterate over all descriptors in this config */
    int offset = cfg->bLength; // typically 9
    while (offset + 2 <= config_len)
    {
        const uint8_t *p = &config_buf[offset];
        uint8_t bLength = p[0];
        uint8_t bDescriptorType = p[1];

        if ((offset + bLength) > config_len || bLength < 2)
            break; // malformed descriptor

        if (bDescriptorType == 4) /* INTERFACE descriptor */
        {
            const struct usb_interface_descriptor *intf =
                (const struct usb_interface_descriptor *)p;
            if (intf->bInterfaceClass == 0x07) // Printer class
            {
                // Next descriptors until next interface or end are endpoints, etc.
                // We want to find a Bulk OUT endpoint
                int ep_offset = offset + bLength;
                int found = 0;
                for (int e = 0; e < intf->bNumEndpoints; e++)
                {
                    if ((ep_offset + sizeof(struct usb_endpoint_descriptor)) > config_len)
                        break;
                    const struct usb_endpoint_descriptor *ep =
                        (const struct usb_endpoint_descriptor *)(&config_buf[ep_offset]);
                    if (ep->bDescriptorType == 5) /* ENDPOINT descriptor */
                    {
                        uint8_t ep_addr = ep->bEndpointAddress; // bit7=IN(1)/OUT(0), bits0-3=ep num
                        uint8_t ep_attr = ep->bmAttributes & 0x3; // 0=Control,1=Iso,2=Bulk,3=Interrupt
                        if (((ep_addr & 0x80) == 0x00) && // OUT
                            (ep_attr == 2))              // Bulk
                        {
                            *out_endpoint = (ep_addr & 0x0F); // ep number
                            return 0; // success
                        }
                    }
                    ep_offset += ep->bLength;
                }
                if (found) break;
            }
        }

        offset += bLength;
    }

    return -1;
}

/* ---------------------- Bulk Transfer Routine ---------------------- */

/* Very naive function to do a single bulk OUT transfer on a given channel
   to a given endpoint. Data is 'Hello\n'. This reuses the existing naive
   approach from your example, but let's generalize the endpoint number. */
static void send_bulk_out_hello(uint8_t device_addr, uint8_t endpoint_num, uint32_t channel)
{
    static const char helloData[] = "Hello\n";
    const uint32_t dataLen = sizeof(helloData) - 1; // 6 bytes

    // 1) Clear channel interrupts
    mmio_write(DWC_HCINT(channel), 0x7FF);

    // 2) Configure transfer size in HCTSIZ
    //    Packet count = 1, data length = 6, PID=DATA0 or DATA1 – for bulk we typically start with DATA0
    mmio_write(DWC_HCTSIZ(channel),
        ((1 << 19) |               /* Packet count = 1 */
         (dataLen & 0x7FFFF))      /* transfer size */
        /* For Bulk OUT on full-speed, PID often starts at DATA0 => bits [31:29] = b'0,
           but if you need to force data PID, you'd do so here. */
    );

    // 3) The buffer pointer for DMA
    mmio_write(DWC_HCDMA(channel), (uint32_t)(uintptr_t)helloData);

    // 4) Endpoint number and direction in HCCHAR
    //    MPS (max packet size)  = let's assume 64 for full-speed bulk
    //    DevAddr = device_addr
    //    EPNUM = endpoint_num, EPDIR=0(OUT), EPTYPE=2(bulk), LSDEV=0, CHENA=1
    uint32_t hcchar_val = (64 << 0) |                   /* MPS=64 */
                          ((uint32_t)device_addr << 22) /* Device address */
                          /* NOTE: some DWC variants store DevAddr in bits [31:22].
                             Double-check your documentation. */
                          | (endpoint_num << 11)        /* Endpoint # */
                          | (0 << 15)                   /* EPDIR=0 => OUT */
                          | (2 << 18)                   /* Bulk = 2 */
                          | (1 << 20)                   /* LSDEV=0 => FS device in some docs. 
                                                         * Possibly FSDEV=0 if high speed is not used. 
                                                         */
                          | (1 << 31);                  /* CHENA=1 to enable channel */

    mmio_write(DWC_HCCHAR(channel), hcchar_val);

    // 5) Wait for transfer completion
    if (wait_for_channel_complete(channel) == 0)
    {
        printk("Bulk OUT transfer completed successfully.\n");
    }
    else
    {
        printk("Bulk OUT transfer error.\n");
    }
}

/* ---------------------- Main Routine ---------------------- */

void notmain(void)
{
    printk("Starting USB Host...\n");

    // 1. Initialize the USB block in host mode
    usb_host_init();
    printk("Host core initialized.\n");

    // 2. Wait for device to connect
    wait_for_device_connection();
    printk("Got device connection.\n");

    // 3. Reset the port
    reset_port();
    printk("Port reset done.\n");

    // 4. Enumerate device on address 0, then set it to address 1
    //    a) Get device descriptor (8 bytes first to read bMaxPacketSize0 or up to 18)
    //       But we'll read all 18 in one go for simplicity, ignoring the two-stage approach.
    uint8_t devdesc_buf[18] __attribute__((aligned(4)));
    for (int i=0; i<18; i++) devdesc_buf[i] = 0; // clear

    if (get_descriptor(0, 1 /* DEVICE */, 0 /* index=0 */, devdesc_buf, 18) != 0) {
        printk("Error: GET_DESCRIPTOR(DEVICE) failed\n");
        return;
    }

    struct usb_device_descriptor *devdesc = (struct usb_device_descriptor *)devdesc_buf;
    printk("Device Descriptor: VID=%04x PID=%04x, MPS0=%d, NumCfg=%d\n",
           devdesc->idVendor, devdesc->idProduct,
           devdesc->bMaxPacketSize0, devdesc->bNumConfigurations);

    // b) Set address to 1
    if (set_address(0, 1) != 0) {
        printk("Error: SET_ADDRESS failed\n");
        return;
    }
    printk("Set address to 1.\n");

    // 5. Get configuration descriptor
    //    We'll read up to, say, 256 bytes for simplicity.
    //    In reality, you'd read wTotalLength from the first 9 bytes, then read exactly that length.
    uint8_t config_buf[256] __attribute__((aligned(4)));
    for (int i=0; i<256; i++) config_buf[i] = 0;

    if (get_descriptor(1, 2 /* CONFIGURATION */, 0 /* index=0 */, config_buf, 256) != 0) {
        printk("Error: GET_DESCRIPTOR(CONFIGURATION) failed\n");
        return;
    }

    // 6. Parse config descriptor to find a printer interface and Bulk OUT endpoint
    uint8_t bulk_out_ep = 0;
    uint8_t config_value = 0;
    if (find_printer_bulk_out_endpoint(config_buf, 256, &bulk_out_ep, &config_value) != 0)
    {
        printk("Error: no printer bulk-out interface found.\n");
        return;
    }
    printk("Found printer interface, Bulk OUT ep=%d, config=%d\n", bulk_out_ep, config_value);

    // 7. Set configuration (the one we found)
    if (set_configuration(1, config_value) != 0) {
        printk("Error: SET_CONFIGURATION failed\n");
        return;
    }
    printk("Set configuration done.\n");

    // 8. Finally, send "Hello\n" on the discovered Bulk OUT endpoint using channel 0
    send_bulk_out_hello(1 /* device addr */, bulk_out_ep, 0 /* channel */);

    printk("Done sending data. End of notmain().\n");
}
