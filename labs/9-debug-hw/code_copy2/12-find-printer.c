#define NO_HEADERS
#include <rpi.h>
#include <csud-platform.c>
#include <csud/include/types.h>
#include <csud/include/usbd/descriptors.h>
#include <csud/include/usbd/device.h>
#include <csud/include/usbd/devicerequest.h>
#include <csud/include/usbd/pipe.h>
#include <csud/include/usbd/usbd.h>
#include <csud/include/device/hub.h>
#include <stddef.h>

// Function to check if a device is a printer
bool is_printer(struct UsbDevice *device) {
    // Check all interfaces for printer class
    for (int i = 0; i < MaxInterfacesPerDevice; i++) {
        if (device->Interfaces[i].Class == InterfaceClassPrinter) {
            return true;
        }
    }
    return false;
}

// Function to recursively search for printers in the USB device tree
void find_printers_recursive(struct UsbDevice *device, int depth) {
    if (device == NULL) return;
    
    // Only process devices that are fully configured
    if (device->Status == Configured) {
        // Check if this device is a printer
        if (is_printer(device)) {
            printk("Found printer at address %d:\n", device->Number);
            printk("  VendorID: 0x%x\n", device->Descriptor.VendorId);
            printk("  ProductID: 0x%x\n", device->Descriptor.ProductId);
            printk("  USB Version: %x\n", device->Descriptor.UsbVersion);
            printk("  Device Class: %d\n", device->Descriptor.Class);
            printk("  Max Packet Size: %d\n", device->Descriptor.MaxPacketSize0);
            printk("  Speed: %s\n", SpeedToChar(device->Speed));
            printk("  Description: %s\n", UsbGetDescription(device));
        }
    }
    
    // If this is a hub, check all its children
    struct HubDevice *hubDevice = (struct HubDevice*)device->DriverData;
    if (hubDevice != NULL && hubDevice->Header.DeviceDriver == DeviceDriverHub) {
        for (int i = 0; i < hubDevice->MaxChildren; i++) {
            if (hubDevice->Children[i] != NULL) {
                find_printers_recursive(hubDevice->Children[i], depth + 1);
            }
        }
    }
}

// Main function to find all printers
void find_printers() {
    printk("Searching for USB printers...\n");
    
    // Get the root hub and start the search
    struct UsbDevice *rootHub = UsbGetRootHub();
    if (rootHub == NULL) {
        printk("Error: Could not get root hub\n");
        return;
    }
    
    find_printers_recursive(rootHub, 0);
    printk("Printer search complete\n");
}

void list_devices_recursive(struct UsbDevice *device, int depth) {
    if (device == NULL) return;
    
    // Print indentation based on depth
    for (int i = 0; i < depth; i++) printk("  ");
    
    printk("Device at address %d:\n", device->Number);
    for (int i = 0; i < depth; i++) printk("  ");
    printk("  Status: %d, VendorID: 0x%x, ProductID: 0x%x\n", 
           device->Status, device->Descriptor.VendorId, device->Descriptor.ProductId);
    for (int i = 0; i < depth; i++) printk("  ");
    printk("  Class: %d, Description: %s\n", device->Descriptor.Class, UsbGetDescription(device));
    
    // If this is a hub, check all its children
    struct HubDevice *hubDevice = (struct HubDevice*)device->DriverData;
    if (hubDevice != NULL && hubDevice->Header.DeviceDriver == DeviceDriverHub) {
        for (int i = 0; i < hubDevice->MaxChildren; i++) {
            if (hubDevice->Children[i] != NULL) {
                list_devices_recursive(hubDevice->Children[i], depth + 1);
            }
        }
    }
}

// Function to list all USB devices
void list_all_devices() {
    printk("Listing all USB devices...\n");
    
    struct UsbDevice *rootHub = UsbGetRootHub();
    if (rootHub == NULL) {
        printk("Error: Could not get root hub\n");
        return;
    }
    
    list_devices_recursive(rootHub, 0);
    printk("Device listing complete\n");
}



void notmain() {
    kmalloc_init(100);
    printk("Starting USB initialization...\n");
    
    // Initialize the USB subsystem
    if (UsbInitialise() != OK) {
        printk("Failed to initialize USB\n");
        return;
    }
    
    printk("USB initialized successfully\n");
    
    // Wait longer for devices to be enumerated (3 seconds)
    printk("Waiting for device enumeration...\n");
    delay_ms(3000);
    
    // Look for printers
    list_all_devices();
    find_printers();
    
    printk("USB search complete\n");
}