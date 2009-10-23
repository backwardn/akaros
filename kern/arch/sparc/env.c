/* See COPYRIGHT for copyright information. */
#ifdef __SHARC__
#pragma nosharc
#endif

#ifdef __DEPUTY__
#pragma noasync
#endif

#include <arch/trap.h>
#include <env.h>
#include <assert.h>
#include <arch/arch.h>
#include <pmap.h>

void
( env_push_ancillary_state)(env_t* e)
{
	static_assert(offsetof(ancillary_state_t,fpr) % 8 == 0);

	#define push_two_fp_regs(pdest,n) \
	    __asm__ __volatile__ ("std	%%f" XSTR(n) ",[%0+4*" XSTR(n) "]" \
	                      : : "r"(pdest) : "memory");

	if(e->env_tf.psr & PSR_EF)
	{
		write_psr(read_psr() | PSR_EF);

		e->env_ancillary_state.fsr = read_fsr();

		push_two_fp_regs(e->env_ancillary_state.fpr,0);
		push_two_fp_regs(e->env_ancillary_state.fpr,2);
		push_two_fp_regs(e->env_ancillary_state.fpr,4);
		push_two_fp_regs(e->env_ancillary_state.fpr,6);
		push_two_fp_regs(e->env_ancillary_state.fpr,8);
		push_two_fp_regs(e->env_ancillary_state.fpr,10);
		push_two_fp_regs(e->env_ancillary_state.fpr,12);
		push_two_fp_regs(e->env_ancillary_state.fpr,14);
		push_two_fp_regs(e->env_ancillary_state.fpr,16);
		push_two_fp_regs(e->env_ancillary_state.fpr,18);
		push_two_fp_regs(e->env_ancillary_state.fpr,20);
		push_two_fp_regs(e->env_ancillary_state.fpr,22);
		push_two_fp_regs(e->env_ancillary_state.fpr,24);
		push_two_fp_regs(e->env_ancillary_state.fpr,26);
		push_two_fp_regs(e->env_ancillary_state.fpr,28);
		push_two_fp_regs(e->env_ancillary_state.fpr,30);

		write_psr(read_psr() & ~PSR_EF);
	}
}

void
( env_pop_ancillary_state)(env_t* e)
{ 

	#define pop_two_fp_regs(pdest,n) \
	    __asm__ __volatile__ ("ldd	[%0+4*" XSTR(n) "], %%f" XSTR(n) \
	                      : : "r"(pdest) : "memory");

	if(e->env_tf.psr & PSR_EF)
	{
		write_psr(read_psr() | PSR_EF);

		pop_two_fp_regs(e->env_ancillary_state.fpr,0);
		pop_two_fp_regs(e->env_ancillary_state.fpr,2);
		pop_two_fp_regs(e->env_ancillary_state.fpr,4);
		pop_two_fp_regs(e->env_ancillary_state.fpr,6);
		pop_two_fp_regs(e->env_ancillary_state.fpr,8);
		pop_two_fp_regs(e->env_ancillary_state.fpr,10);
		pop_two_fp_regs(e->env_ancillary_state.fpr,12);
		pop_two_fp_regs(e->env_ancillary_state.fpr,14);
		pop_two_fp_regs(e->env_ancillary_state.fpr,16);
		pop_two_fp_regs(e->env_ancillary_state.fpr,18);
		pop_two_fp_regs(e->env_ancillary_state.fpr,20);
		pop_two_fp_regs(e->env_ancillary_state.fpr,22);
		pop_two_fp_regs(e->env_ancillary_state.fpr,24);
		pop_two_fp_regs(e->env_ancillary_state.fpr,26);
		pop_two_fp_regs(e->env_ancillary_state.fpr,28);
		pop_two_fp_regs(e->env_ancillary_state.fpr,30);

		write_fsr(e->env_ancillary_state.fsr);

		write_psr(read_psr() & ~PSR_EF);
	}
}


// Flush all mapped pages in the user portion of the address space
// TODO: only supports L3 user pages
void
env_user_mem_free(env_t* e)
{
	pte_t *l1pt = e->env_pgdir, *l2pt, *l3pt;
	uint32_t l1x,l2x,l3x;
	physaddr_t l2ptpa,l3ptpa,page_pa;
	uint32_t l2_tables_per_page,l3_tables_per_page;

	l2_tables_per_page = PGSIZE/(sizeof(pte_t)*NL2ENTRIES);
	l3_tables_per_page = PGSIZE/(sizeof(pte_t)*NL3ENTRIES);

	static_assert(L2X(UTOP) == 0 && L3X(UTOP) == 0);
	for(l1x = 0; l1x < L1X(UTOP); l1x++)
	{
		if(!(l1pt[l1x] & PTE_PTD))
			continue;

		l2ptpa = PTD_ADDR(l1pt[l1x]);
		l2pt = (pte_t*COUNT(NL2ENTRIES)) KADDR(l2ptpa);

		for(l2x = 0; l2x < NL2ENTRIES; l2x++)
		{
			if(!(l2pt[l2x] & PTE_PTD))
				continue;

			l3ptpa = PTD_ADDR(l2pt[l2x]);
			l3pt = (pte_t*COUNT(NL3ENTRIES)) KADDR(l3ptpa);

			for(l3x = 0; l3x < NL3ENTRIES; l3x++)
			{
				if(l3pt[l3x] & PTE_PTE)
				{
					page_pa = PTE_ADDR(l3pt[l3x]);
					l3pt[l3x] = 0;
					page_decref(pa2page(page_pa));
				}
			}

			l2pt[l2x] = 0;

			// free the L3 PT itself
			page_decref(pa2page(l3ptpa));
		}

		l1pt[l1x] = 0;

		// free the L2 PT itself
		page_decref(pa2page(l2ptpa));
	}

	tlbflush();
}
