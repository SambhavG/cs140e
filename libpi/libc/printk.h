#ifndef __PRINTK_H__
#define __PRINTK_H__
#include "rpi.h"

static void emit_val(unsigned base, uint32_t u);
int vprintk(const char *fmt, va_list ap);
int printk(const char *fmt, ...);

#endif