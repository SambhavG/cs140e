#include "rpi.h"

#define USB_BASE 0x20980000


#define GOTGCTL USB_BASE + 0x000
#define GOTGINT USB_BASE + 0x004
#define GAHBCFG USB_BASE + 0x008
#define GUSBCFG USB_BASE + 0x00C
#define GRSTCTL USB_BASE + 0x010
#define GINTSTS USB_BASE + 0x014
#define GINTMSK USB_BASE + 0x018
#define GRXSTSR USB_BASE + 0x01C
#define GRXSTSP USB_BASE + 0x020
#define GRXFSIZ USB_BASE + 0x024
#define GNPTXFSIZ USB_BASE + 0x028
#define GNPTXSTS USB_BASE + 0x02C
#define GDFIFOCFG USB_BASE + 0x05C
#define HPTXFSIZ USB_BASE + 0x100
#define DIEPTXF1 USB_BASE + 0x104
#define DIEPTXF2 USB_BASE + 0x108
#define DIEPTXF3 USB_BASE + 0x10C
#define DIEPTXF4 USB_BASE + 0x110
#define DIEPTXF5      USB_BASE + 0x114
#define DIEPTXF6      USB_BASE + 0x118
#define HCFG          USB_BASE + 0x400
#define HFIR          USB_BASE + 0x404
#define HFNUM         USB_BASE + 0x408
#define HPTXSTS       USB_BASE + 0x410
#define HAINT         USB_BASE + 0x414
#define HAINTMSK      USB_BASE + 0x418
#define HPRT          USB_BASE + 0x440
#define HC0_CHAR      USB_BASE + 0x500
#define HC0_INT       USB_BASE + 0x508
#define HC0_INTMSK    USB_BASE + 0x50C
#define HC0_TSIZ      USB_BASE + 0x510
#define HC0_DMAADDR   USB_BASE + 0x514
#define DCFG          USB_BASE + 0x800
#define DCTL          USB_BASE + 0x804
#define DSTS          USB_BASE + 0x808
#define DIEPMSK       USB_BASE + 0x810
#define DOEPMSK       USB_BASE + 0x814
#define DAINT         USB_BASE + 0x818
#define DAINTMSK      USB_BASE + 0x81C
#define DVBUSDIS      USB_BASE + 0x828
#define DVBUSPULSE    USB_BASE + 0x82C
#define DIEPEMPMSK    USB_BASE + 0x834
#define DIEP0CTL      USB_BASE + 0x900
#define DIEP0INT      USB_BASE + 0x908
#define DIEP0TSIZ     USB_BASE + 0x910
#define DIEP0DMAADDR  USB_BASE + 0x914
#define DIEP0TXFSTS   USB_BASE + 0x918
#define DIEP0_CTL     USB_BASE + 0x920
#define DIEP0_INT     USB_BASE + 0x928
#define DIEP0_TSIZ    USB_BASE + 0x930
#define DIEP0_DMAADDR USB_BASE + 0x934
#define DIEP0_TXFSTS  USB_BASE + 0x938
#define DIEP1_CTL     USB_BASE + 0x940
#define DIEP1_INT     USB_BASE + 0x948
#define DIEP1_TSIZ    USB_BASE + 0x950
#define DIEP1_DMAADDR USB_BASE + 0x954
#define DIEP1_TXFSTS  USB_BASE + 0x958
#define DOEP0_CTL     USB_BASE + 0xB00
#define DOEP0_INT     USB_BASE + 0xB08
#define DOEP0_TSIZ    USB_BASE + 0xB10
#define DOEP0_DMAADDR USB_BASE + 0xB14
#define DOEP0_TXFSTS  USB_BASE + 0xB18

#define HPRT_CONNSTS (1 << 0)    // Port status
#define HPRT_RST (1 << 8)        // Port Reset
#define HPRT_PWR (1 << 12)       // Port Power
#define HPRT_PRTSPD (0b11 << 17) // Port speed

#define GINTMSK_PRTINTMSK (1 << 24) // Port status
// set all the device masks Enumeration Done, USB Reset, USB Suspend, Early Suspend
#define GINTMSK_DEVICEMASKS (0b1111 << 10)

#define HCFG_FSLSUPP (1 << 2)        // full speed host
#define HCFG_FSLSPCLKSEL (0b11 << 0) // physical clock speed

#define HFIR_FRINT ((0b1 << 16) - 1) // bits to store the frame interval

#define DCFG_DEVSPD 0b11           // speed of device
#define DCFG_NZSTSOUTHSHK 0b1 << 2 // non-zero length of status OUT handshake
#define DCFG_PERFRINT 0b11 << 11   // periodic frame interval


#define DSTS_SUSPSTS 0b1                            // suspend status
#define DSTS_ENUMSPD 0b11 << 1                      // device enumerated speed
#define DSTS_SOFFN ((0b1 << (21 - 8 + 1)) - 1) << 8 // device received SOF

#define DOEP0_CTL_EPENA (1 << 31)

void get_gintmsk(void) {
    unsigned gintmsk = GET32(GINTMSK);
    for (int i = 0; i < 32; i++) {
        unsigned val = (gintmsk >> i) & 1;
        if (val == 1) {
            switch (i) {
                case 1:
                    printk("MODEMISMSK is set\n");
                    break;
                case 2:
                    printk("OTGINTMSK is set\n");
                    break;
                case 3:
                    printk("SOFMSK is set\n");
                    break;
                case 4:
                    printk("RXFLVLMSK is set\n");
                    break;
                case 5:
                    printk("NPTXFEMPMSK is set\n");
                    break;
                case 6:
                    printk("PTXFEMPMSK is set\n");
                    break;
                case 7:
                    printk("GOUTNAKEFFMSK is set\n");
                    break;
                case 10:
                    printk("ERLYSUSPMSK is set\n");
                    break;
                case 11:
                    printk("USBSUSPMSK is set\n");
                    break;
                case 12:
                    printk("USBRSTMSK is set\n");
                    break;
                case 13:
                    printk("ENUMDONEMSK is set\n");
                    break;
                case 14:
                    printk("ISOOUTDROPMSK is set\n");
                    break;
                case 15:
                    printk("EOPFMSK is set\n");
                    break;
                case 18:
                    printk("IEPINTMSK is set\n");
                    break;
                case 19:
                    printk("OEPINTMSK is set\n");
                    break;
                case 20:
                    printk("INCOMPISOINMSK is set\n");
                    break;
                case 21:
                    printk("INCOMPLPMSK is set\n");
                    break;
                case 22:
                    printk("FETSUSPMSK is set\n");
                    break;
                case 23:
                    printk("RESETDETMSK is set\n");
                    break;
                case 24:
                    printk("PRTINTMSK is set\n");
                    break;
                case 25:
                    printk("HCHINTMSK is set\n");
                    break;
                case 26:
                    printk("PTXFEMPMSK is set\n");
                    break;
                case 28:
                    printk("CONIDSTSCHNGMSK is set\n");
                    break;
                case 29:
                    printk("DISCONNINTMSK is set\n");
                    break;
                case 30:
                    printk("SESSREQINTMSK is set\n");
                    break;
                case 31:
                    printk("WKUPINTMSK is set\n");
                    break;
            }
        }   
    }
    printk("\n");
}

void notmain(void)
{
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

    // implementing the protocol in section 15.4.1.2 device initialization

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

    for (int i = 0; i < 5; i++)
    {
        dsts = GET32(DSTS) >> 8;
        printk("Device status register frame number: %u\n", dsts);
    }

    //enable the endpoint
    PUT32(DOEP0_CTL, GET32(DOEP0_CTL) | DOEP0_CTL_EPENA);
    unsigned doep0_ctl = GET32(DOEP0_CTL);
    printk("DOEP0_CTL: %b\n", doep0_ctl);

    unsigned doep0_int = GET32(DOEP0_INT);
    printk("DOEP0_INT: %b\n", doep0_int);

    unsigned doep0_tsiz = GET32(DOEP0_TSIZ);
    printk("DOEP0_TSIZ: %b\n", doep0_tsiz);

    unsigned doep0_dmaaddr = GET32(DOEP0_DMAADDR);
    printk("DOEP0_DMAADDR: %x\n", doep0_dmaaddr);

    for (int j = 0; j < 10; j++) {
        for (int i = 0; i < 5; i++) {
            unsigned val = GET32(doep0_dmaaddr + i * 4);
            printk("*DOEP0_DMAADDR: %x\n", val);
        }
        printk("\n");
        delay_ms(100);
    }

    get_gintmsk();
    


}