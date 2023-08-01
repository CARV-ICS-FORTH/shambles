#ifndef ARCH_H
#define ARCH_H

#ifdef __x86_64__
#include "arch/x86.h"
#endif

#endif

#ifndef SPECIFIC_ARCH_H_INCLUDED
#error "Current architecture is not supported"
#endif
