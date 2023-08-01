#ifndef ARCH_H
#error "This file should  not be included directly"
#endif

#ifndef SPECIFIC_ARCH_H_INCLUDED
#define SPECIFIC_ARCH_H_INCLUDED

#define DEFINE_COUNT(varName) register uint64_t varName asm ("r15")
#define DEFINE_ADDR(varName) register void* varName asm ("r14")
#define DEFINE_COUNTER(varName) register uint64_t varName asm ("r13")
#define DEFINE_INC(varName) register uint64_t varName asm ("r12")
#define DEFINE_OFFT(varName) register uint64_t varName asm ("r11")	//XXX typically caller saved, but we are out of registers
#define DEFINE_MASK(varName) register uint64_t varName asm ("rbx")
#define DEFINE_TMP(varName) register uint64_t varName asm ("rbp")

#define BURN_CYCLES(count, i) {\
	i = 0;\
	asm volatile(\
		"loop%=:\n"\
		"inc %0\n"\
		"cmp %0, %1\n"\
		"jne loop%="\
	::"r"(i), "r"(count):);\
}

#define LOAD_64(addr, offset, tmp) {\
	asm volatile(\
		"mov %1, %0\n"\
		"add %2, %0\n"\
		"mov (%0), %0\n"\
	:"=r"(tmp):"r"(addr), "r"(offset):);\
}

#define STORE_64(addr, offset, tmp) {\
	asm volatile(\
		"mov %1, %0\n"\
		"add %2, %0\n"\
		"mov %0, (%0)\n"\
	:"=r"(tmp):"r"(addr), "r"(offset):);\
}

#define NMOP(addr, offset, tmp) {\
	asm volatile(\
		"mov %1, %0\n"\
		"add %2, %0\n"\
	:"=r"(tmp):"r"(addr), "r"(offset):);\
}

#define OFFSET(offset, inc, mask) {\
	asm volatile(\
		"add %0, %1\n"\
		"and %0, %2\n"\
	:"=r"(offset):"r"(inc), "r"(mask):);\
}

#else
#error "Arch specific header file already included"
#endif
