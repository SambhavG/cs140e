#ifndef __RPI_MBR_H__
#define __RPI_MBR_H__
#include "mbr-helpers.h"
#include "pi-sd.h"
#include "rpi.h"

// Load the MBR from the disk and verify it.
mbr_t *mbr_read();

#endif
