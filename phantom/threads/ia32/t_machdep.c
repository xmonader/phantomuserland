/**
 *
 * Phantom OS multithreading library.
 *
 * Copyright (C) 2009-2011 Dmitry Zavalishin, dz@dz.ru
 *
 * Machine dependent C code, ia32.
 *
 * Licensed under CPL 1.0, see LICENSE file.
 *
**/

#include <thread_private.h>
#include <phantom_libc.h>

#include <hal.h>

#include <ia32/seg.h>
#include <ia32/eflags.h>
#include <ia32/tss.h>
#include <ia32/private.h>

#include <kernel/ia32/cpu.h>

//#define FXDEBUG(a) a
#define FXDEBUG(a)

// asm code
void phantom_thread_trampoline(void);
//void phantom_thread_fp_init(void *temp, void *newstate );

/**
 * Initializes what is needed for thread to be ready to
 * switch to. Thread structure must be already filled.
 */
void phantom_thread_state_init(phantom_thread_t *t)
{
    t->cpu.esp = (int)(t->stack + t->stack_size);
    t->cpu.ebp = 0;
    t->cpu.eip = (int)phantom_thread_trampoline;
    //t->cpu.flags = 0;

#if CONF_DUAL_PAGEMAP
    //t_set_paged_mem(0); // by default forbid accessing paged mem
    int32_t cr3 = arch_get_pdir( 0 );
    t->cr3 = cr3;
#endif

    int *esp = (int *)(t->cpu.esp);

    // --- Will be popped by thread switch code ---
    // Simulate phantom_switch_context's three params
    STACK_PUSH(esp,0); // ridiculous?
    STACK_PUSH(esp,0);
    STACK_PUSH(esp,0);

    STACK_PUSH(esp,t->cpu.eip); // as if we called func

    STACK_PUSH(esp,t);// EBX
    STACK_PUSH(esp,t->start_func_arg);// EDI
    STACK_PUSH(esp,t->start_func);// ESI
    STACK_PUSH(esp,0);// CR2

    t->cpu.esp = (int)esp;

/*
	// God knows why it works just once for importing main thread,
	// then causes 'no fpu' exception 7 err 0
#if 0 && FXSAVE
    //char mystate[512];
    //char his_state[512];

	// We need two 16 bytes alinged buffers
	char state_buffer[512*2 + 16];  

	char  *my_state = state_buffer;

	while( ((unsigned)my_state) & 0xFu )
		my_state++;

	char *his_state = my_state+512;

    //phantom_thread_fp_init( mystate, t->cpu.fxstate );
//FXDEBUG(double x = 0.0 + 1.0);

    //asm volatile("fxsave %0" : : "m" (my_state));
    i386_fxsave(my_state);
//FXDEBUG(hexdump( &mystate, 512, "FXSTATE our", 0));
    asm volatile("finit " : : );

    //asm volatile("fxsave %0" : : "m" (his_state));
    i386_fxsave(his_state);
//FXDEBUG(hexdump( &mystate, 512, "FXSTATE init", 0));

    i386_fxrstor(my_state);
    //asm volatile("fxrstor %0" : : "m" (my_state));

#endif
*/
}

void switch_to_user_mode()
{
    /**
     *    Push order:
     *
     *    SS
     *    ESP
     *    EFLAGS
     *    CS
     *    EIP
     *
    **/

    // Set up a stack structure for switching to user mode.
    asm volatile("  \
                 sti; \
                 mov %1, %%ax; \
                 mov %%ax, %%ds; \
                 mov %%ax, %%es; \
                 mov %%ax, %%fs; \
                 mov %%ax, %%gs; \
                 \
                 mov %%esp, %%eax; \
                 pushl %1; \
                 pushl %%eax; \
                 pushf; \
                 pushl %0; \
                 push $1f; \
                 iret; \
                 1: \
                 "
                 : : "i" (USER_CS), "i" (USER_DS)
                );
}


// Do what is required (arch specific) after switching to a new thread
void arch_adjust_after_thread_switch(phantom_thread_t *t)
{
#if CONF_DUAL_PAGEMAP    
    if(t->cr3 != 0)
        set_cr3(t->cr3); // flushes TLB too, need G (global) bit to be set on page table entries for kernel range?
    else
        printf(" tid %d CR3 == %x ", t->tid, t->cr3);
    
#endif

//#warning not SMP compliant
// must find out which TSS is for our CPU and update it's esp0
    int ncpu = GET_CPU_ID();
    cpu_tss[ncpu].esp0 = (addr_t)t->kstack_top;

    //t->cpu_id = ncpu;

    // NO! - kill that "Or else CPU doesn't take in account esp0 change :("
    //phantom_load_main_tss();
    //phantom_load_cpu_tss(ncpu);
}




void dump_thread_stack(phantom_thread_t *t)
{
    void *ebp = (void *)t->cpu.ebp;
    printf("Thread %d EIP 0x%08X, ", t->tid, t->cpu.eip);
    stack_dump_from(ebp);
}
