#include "rpi.h"

#define USB_BASE 0x20980000

#define HPRT USB_BASE + 0x440    // Host Port Control & Status
#define HPRT_CONNSTS (1 << 0)    // Port status
#define HPRT_RST (1 << 8)        // Port Reset
#define HPRT_PWR (1 << 12)       // Port Power
#define HPRT_PRTSPD (0b11 << 17) // Port speed

#define GINTMSK USB_BASE + 0x018    // Mask to enable interrupts
#define GINTMSK_PRTINTMSK (1 << 24) // Port status
// set all the device masks Enumeration Done, USB Reset, USB Suspend, Early Suspend
#define GINTMSK_DEVICEMASKS (0b1111 << 10)
#define GINTMSK_RXFLVLMSK (0b1 << 4)  // receive FIFO non-empty mask host and device
#define GINTMSK_OTGINTMSK (0b1 << 2)  // OTG interrupt mask
#define GINTMSK_MODEMISMSK (0b1 << 1) // Mode mismatch interrupt mask

#define GAHBCFG USB_BASE + 0x008 // Global AHB Configuration Register
// ahb burst length, global interrupt mask bit = 1,
#define GAHBCFG_DMAEN (1 << 5)        // 1 is DMA mode, 0 is slave mode
#define GAHBCFG_HBSTLEN (0b1111 << 1) // Burst length/type
#define GAHBCFG_GLBINTRMSK 0b1        // Global interrupts enabled mask

#define GUSBCFG USB_BASE + 0x00C // USB configuration register

#define HCFG USB_BASE + 0x400        // Host Configuration Register
#define HCFG_FSLSUPP (1 << 2)        // full speed host
#define HCFG_FSLSPCLKSEL (0b11 << 0) // physical clock speed

#define HFIR USB_BASE + 0x404        // Host frame interval Register
#define HFIR_FRINT ((0b1 << 16) - 1) // bits to store the frame interval

#define DCFG USB_BASE + 0x800      // device configuration register
#define DCFG_DEVSPD 0b11           // speed of device
#define DCFG_NZSTSOUTHSHK 0b1 << 2 // non-zero length of status OUT handshake
#define DCFG_PERFRINT 0b11 << 11   // periodic frame interval

#define DSTS USB_BASE + 0x808                       // device status register
#define DSTS_SUSPSTS 0b1                            // suspend status
#define DSTS_ENUMSPD 0b11 << 1                      // device enumerated speed
#define DSTS_SOFFN ((0b1 << (21 - 8 + 1)) - 1) << 8 // device received SOF

#define DIEP0_CTL USB_BASE + 0x900    // Device IN endpoint control register
#define DIEP0_CTL_ENABLED (0b1 << 31) // bit to check it is enabled

#define DOEP0_CTL USB_BASE + 0xB20    // Device OUT endpoint control register
#define DOEP0_CTL_ENABLED (0b1 << 31) // bit to check it is enabled

#define HAINTMSK USB_BASE + 0x418 // Host all channels interrupt mask register
#define HAINTMSK_CHL0 0x1         // toggle channel 0 interrupts

#define HC0_INTMSK USB_BASE + 0x50C  // Channel 0 interrupt mask register
#define HC0_INTMSK_XACTERRMSK 1 << 7 // transaction error mask

#define HC0_CHAR USB_BASE + 0x500   // Channel 0 charcteristics register
#define HC0_CHENA 1 << 31           // channel enable
#define HC0_DEVADDR 0b1111111 << 22 // device address
#define HC0_EPTYPE 0b11 << 18       // endpoint type
#define HC0_EPDIR 0b1 << 15         // indicates if the transaction is in or out
#define HC0_EPN (0b1111 << 11)      // endpoint number
#define HC0_MPS ((0b1 << 11) - 1)   // max packet size

#define HC0_TSIZ USB_BASE + 0x510                          // Channel 0 transfer size register
#define HC0_TSIZ_PID 0b11 << 29                            // packet ID
#define HC0_TSIZ_PKTCNT ((0b1 << (28 - 19 + 1)) - 1) << 19 // packet count
#define HC0_TSIZ_XFERSIZE ((0b1 << 19) - 1)                // transfer size

#define GRXFSIZ USB_BASE + 0x024
#define GNPTXFSIZ USB_BASE + 0x028
#define GNPTXSTS USB_BASE + 0x02C
void init(void)
{
    // implementing the global initialization protocol in 15.4.1

    // NOTE NOTE NOTE setting the last bit to 0 will enable interrupts
    // RIGHT NOW INTERRUPTS ARE GLOBALLY OFF WHICH IS OKAY (??)
    // I'm not setting the FIFO empty bits either
    // also, we are staying in slave mode
    // PUT32(GAHBCFG, GET32(GAHBCFG) | GAHBCFG_GLBINTRMSK);

    // global step 2. enable receive fifo
    PUT32(GINTMSK, (GET32(GINTMSK) & ~GINTMSK_RXFLVLMSK) | GINTMSK_MODEMISMSK | GINTMSK_OTGINTMSK);

    // implementing the protocol in section 15.4.1.1 host initialization

    printk("Pre-configuration device status register: %b\n", GET32(DSTS));
    unsigned dsts_pre;
    for (int i = 0; i < 2; i++)
    {
        delay_ms(1000);
        dsts_pre = GET32(DSTS);
        printk("Device status register 1s later: %b, %x\n", dsts_pre, dsts_pre);
    }
    printk("\n");

    // 1. enable GINTMSK PRTINT
    PUT32(GINTMSK, GET32(GINTMSK) | GINTMSK_PRTINTMSK);

    // 2. enable full speed by setting this to 0
    PUT32(HCFG, GET32(HCFG) & ~HCFG_FSLSUPP);
    // (?) in advance of 10, set clock speed to 48mhz
    PUT32(HCFG, (GET32(HCFG) & ~HCFG_FSLSPCLKSEL) | 0b1);

    // 3. Power on the port
    PUT32(HPRT, GET32(HPRT) | HPRT_PWR);

    // 4. we don't need to do this,
    // we will assume that it is already connected
    // instead of waiting for interrupts

    // *** IMPLEMENT the protocol in section 15.4.1.2 device initialization

    // 1. device configuration
    unsigned dcfg = GET32(DCFG);
    dcfg &= ~DCFG_DEVSPD;
    dcfg |= 3;
    dcfg &= ~DCFG_NZSTSOUTHSHK; // this is a guess
    dcfg &= ~DCFG_PERFRINT;
    dcfg |= 0b10 << 11; // this is a guess
    PUT32(DCFG, dcfg);

    // 2. enable interrupts
    PUT32(GINTMSK, GET32(GINTMSK) | GINTMSK_DEVICEMASKS);

    // 3. it seems like the USB CFG is fine already

    // *** DONE implementing device initialization protocol

    // *** IMPLEMENTING channel initialization protocol for channel 0

    // 1/2 -- don't need to be done because we do not have a packet count of more than 1

    // 3. unmask channel 0's interrupts
    PUT32(HAINTMSK, GET32(HAINTMSK) | HAINTMSK_CHL0);

    // 4. unmask the transaction-related interupts
    PUT32(HC0_INTMSK, GET32(HC0_INTMSK) | HC0_INTMSK_XACTERRMSK);

    // 5. program the transfer size register
    unsigned xfer = 0;
    // pid is data 0
    xfer |= HC0_TSIZ_PKTCNT; // packet count
    xfer |= 0b1000;          // transfer size is one byte?
    PUT32(HC0_TSIZ, xfer);
    // 6. unneeded because we are in slave mode (not DMA mode)

    // 7. program the characteristics register and enable the channel
    unsigned characteristics = 0;
    characteristics |= HC0_CHENA; // enable
    characteristics |= 1 << 22;   // device address is 1
    // we want it to be a control endpoint
    // epdir is 0 so it's out
    characteristics |= 1 << 11; // endpoint number 1
    characteristics |= 1 << 4;  // max packet size
    PUT32(HC0_CHAR, characteristics);

    // *** DONE implementing channel initialization protocol

    delay_ms(100); // Wait for power to stabilize

    // Read initial port status
    unsigned hprt = GET32(HPRT);
    printk("Post-reset HPRT: %b, %x\n", hprt, hprt);

    if (!(hprt & HPRT_CONNSTS))
    {
        printk("ERROR: No device detected...\n");
        return;
    }

    printk("Device detected, starting reset sequence...\n");
    printk("Port enabled: %d\n", (hprt >> 2) & 1);

    // 5. Start port reset
    PUT32(HPRT, GET32(HPRT) | HPRT_RST);

    // 6. Hold reset for 50ms
    delay_ms(50);

    // 7. Release reset
    PUT32(HPRT, GET32(HPRT) & ~HPRT_RST);

    // 8. Wait for device to recover
    // if this is broken, wait for the interrupt HPRT.PRTENCHNG
    delay_ms(20);

    // Check if device is still connected
    hprt = GET32(HPRT);
    if (!(hprt & HPRT_CONNSTS))
    {
        printk("ERROR: Device disconnected during reset!\n");
        return;
    }
    printk("Post-reset HPRT: %b, %x\n", hprt, hprt);
    printk("Port enabled: %d\n", (hprt >> 2) & 1);

    // 9. get enumerated speed
    unsigned speed = GET32(HPRT) >> 17 & 0b11;
    printk("HPRT Print Speed: %b\n", speed);

    // 10. set clock interval to 48000 (1 ms at 48MHZ)
    PUT32(HFIR, (GET32(HFIR) & ~HFIR_FRINT) | 48000);

    // Wait for port to be stable
    delay_ms(100);

    // NOTE NOTE NOTE I AM NOT DOING ANYTHING FOR STEPS 11-13 OF 15.4.1.1
    // THIS IS A PRIME SUSPECT IF THE CODE IS NOT WORKING
    // DONT FORGET DONT FORGET DONT FORGET
    PUT32(GRXFSIZ, 512);
    PUT32(GNPTXFSIZ, 0x01500000 | 512);
    PUT32(GNPTXSTS, 0x400 | 512 << 16);

    // check if the device is connected by reading device status register
    unsigned dsts = GET32(DSTS);
    printk("Post-configuration device status register: %b, %x\n", dsts, dsts);
    printk("(want last bit to be 0, second-last to be 10, and middle bits to be nonzero)\n\n");

    for (int i = 0; i < 2; i++)
    {
        delay_ms(1000);
        dsts = GET32(DSTS);
        printk("Device status register 1s later: %b, %x\n", dsts, dsts);
    }
}

void communicate(void)
{
    // send a packet to the device
    unsigned char init_cmd[] = { 0x1B, 0x40 };
    // write to FIFO queue
    PUT32(0x01500000, init_cmd[0]);
    PUT32(0x01500004, init_cmd[1]);
    delay_ms(1000);
    printk("VALUE: %x\n", GET32(0x01500000));
}

void notmain(void)
{
    init();
    unsigned tsiz = GET32(HC0_TSIZ);
    printk("TSIZ: %b\n", tsiz);
    unsigned characteristics = GET32(HC0_CHAR);
    printk("Characteristics: %b\n", characteristics);
    // unsigned diep0ctl = GET32(DIEP0_CTL);
    // printk("DIEP0CTL: %x, enabled: %x\n", diep0ctl, (diep0ctl & DIEP0_CTL_ENABLED) >> 31);
    unsigned doep0ctl = GET32(DOEP0_CTL);
    printk("DOEP0CTL: %x, enabled: %x\n", doep0ctl, (doep0ctl & DIEP0_CTL_ENABLED) >> 31);
    communicate();
}