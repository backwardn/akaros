#ifndef ROS_ARCH_TRAP_H
#define ROS_ARCH_TRAP_H

#ifdef __riscv64
# define SIZEOF_HW_TRAPFRAME (36*8)
#else
# define SIZEOF_HW_TRAPFRAME (36*4)
#endif

#ifndef __ASSEMBLER__

#ifndef ROS_KERN_TRAP_H
#error "Do not include include arch/trap.h directly"
#endif

#include <ros/trapframe.h>
#include <arch/arch.h>

/* Kernel message interrupt vector.  ignored, for the most part */
#define I_KERNEL_MSG 255

/* For kernel contexts, when we save/restore/move them around. */
struct kernel_ctx {
	/* RISCV's current pop_kernel_ctx assumes the hw_tf is the first member */
	struct hw_trapframe 		hw_tf;
};

static inline bool in_kernel(struct hw_trapframe *hw_tf)
{
	return hw_tf->sr & SR_PS;
}

static inline void __attribute__((always_inline))
set_stack_pointer(uintptr_t sp)
{
	asm volatile("move sp, %0" : : "r"(sp) : "memory");
}

/* Save's the current kernel context into tf, setting the PC to the end of this
 * function.  Note the kernel doesn't need to save a lot.
 * Implemented with extern function to cause compiler to clobber most regs. */
static inline void save_kernel_ctx(struct kernel_ctx *ctx)
{
  extern void save_kernel_tf_asm(struct hw_trapframe*);
	save_kernel_tf_asm(&ctx->hw_tf);
}

void handle_trap(struct hw_trapframe *hw_tf);
int emulate_fpu(struct hw_trapframe *hw_tf);

#endif

#endif
