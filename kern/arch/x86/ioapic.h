/* 
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#ifndef ROS_KERN_IOAPIC_H
#define ROS_KERN_IOAPIC_H
struct bus {
	uint8_t	type;
	uint8_t	busno;
	uint8_t	po;
	uint8_t	el;

	struct aintr*	aintr;			/* interrupts tied to this bus */
	struct bus*	next;
};

struct aintr {
	// no idea yet. PCMPintr* intr;
	struct apic*	apic;
	struct aintr*	next;
};

struct apic {
	int     useable;
	int	type;
	int	apicno;
	uint32_t*	addr;			/* register base address */
	uint32_t	paddr;
	int	flags;			/* PcmpBP|PcmpEN */

	//spinlock_t lock;		/* I/O APIC: register access */
	int	mre;			/* I/O APIC: maximum redirection entry */

	int	lintr[2];		/* Local APIC */
	int	machno;

	int	online;
};

enum {
	MaxAPICNO	= 254,		/* 255 is physical broadcast */
};

enum {					/* I/O APIC registers */
	IoapicID	= 0x00,		/* ID */
	IoapicVER	= 0x01,		/* version */
	IoapicARB	= 0x02,		/* arbitration ID */
	IoapicRDT	= 0x10,		/* redirection table */
};

/*
 * Common bits for
 *	I/O APIC Redirection Table Entry;
 *	Local APIC Local Interrupt Vector Table;
 *	Local APIC Inter-Processor Interrupt;
 *	Local APIC Timer Vector Table.
 */
enum {
	ApicFIXED	= 0x00000000,	/* [10:8] Delivery Mode */
	ApicLOWEST	= 0x00000100,	/* Lowest priority */
	ApicSMI		= 0x00000200,	/* System Management Interrupt */
	ApicRR		= 0x00000300,	/* Remote Read */
	ApicNMI		= 0x00000400,
	ApicINIT	= 0x00000500,	/* INIT/RESET */
	ApicSTARTUP	= 0x00000600,	/* Startup IPI */
	ApicExtINT	= 0x00000700,

	ApicPHYSICAL	= 0x00000000,	/* [11] Destination Mode (RW) */
	ApicLOGICAL	= 0x00000800,

	ApicDELIVS	= 0x00001000,	/* [12] Delivery Status (RO) */
	ApicHIGH	= 0x00000000,	/* [13] Interrupt Input Pin Polarity (RW) */
	ApicLOW		= 0x00002000,
	ApicRemoteIRR	= 0x00004000,	/* [14] Remote IRR (RO) */
	ApicEDGE	= 0x00000000,	/* [15] Trigger Mode (RW) */
	ApicLEVEL	= 0x00008000,
	ApicIMASK	= 0x00010000,	/* [16] Interrupt Mask */
	IOAPIC_PBASE    = 0xfec00000, /* default *physical* address */
};

extern void ioapicinit(struct apic*, int);
extern void ioapicrdtr(struct apic*, int unused_int, int*, int*);
extern void ioapicrdtw(struct apic*, int unused_int, int, int);

#endif /* ROS_KERN_IOAPIC_H */
