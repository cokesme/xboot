/*
 * mmu.c
 */

#include <cp15.h>
#include <cache.h>
#include <mmu.h>

/*
 * Page define
 */
#define PAGE_SIZE			(4096)
#define PAGE_SHIFT			(12)
#define PAGE_ALIGN(x)		(((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(x)	((x) & ~(PAGE_SIZE - 1))

/*
 * Level 1 descriptor (PMD)
 */
enum {
	PMD_TYPE_FAULT		= (0x0 << 0),
	PMD_TYPE_COARSE		= (0x1 << 0),
	PMD_TYPE_SECTION	= (0x2 << 0),
	PMD_TYPE_FINE		= (0x3 << 0),
};

enum {
	PMD_NCNB			= (0x0 << 2),
	PMD_NCB				= (0x1 << 2),
	PMD_CNB				= (0x2 << 2),
	PMD_CB				= (0x3 << 2),
};

enum {
	PMD_AP_FAULT		= (0x0 << 10),
	PMD_AP_SU_ONLY		= (0x1 << 10),
	PMD_AP_USR_RO		= (0x2 << 10),
	PMD_AP_RW			= (0x3 << 10),
};

/*
 * Level 2 descriptor (PTE)
 */
enum {
	PTE_TYPE_FAULT		= (0x0 << 0),
	PTE_TYPE_LARGE		= (0x1 << 0),
	PTE_TYPE_SMALL		= (0x2 << 0),
	PTE_TYPE_TINY		= (0x3 << 0),
};

/*
 * Cache and Write back bit
 */
enum {
	PTE_NCNB			= (0x0 << 2),
	PTE_NCB				= (0x1 << 2),
	PTE_CNB				= (0x2 << 2),
	PTE_CB				= (0x3 << 2),
};

/*
 * MMU translation table base address
 */
static u32_t __mmu_ttb[PAGE_SIZE] __attribute__((aligned(0x4000)));

static void mmu_map_l1_section(virtual_addr_t virt, virtual_size_t size, physical_addr_t phys, u32_t attr)
{
	int i;

	virt >>= 20;
	size >>= 20;
	phys >>= 20;
	attr &= (0x3 << 2);

	for(i = size; i > 0; i--, virt++, phys++)
		__mmu_ttb[virt] = (phys << 20) | (0x1 << 4) | PMD_TYPE_SECTION | PMD_AP_RW | attr;

	__mmu_cache_flush();
}

static inline void tlb_invalidate(void)
{
	asm volatile (
		"mov	r0, #0\n"
		"mcr	p15, 0, r0, c7, c10, 4;	@ drain write buffer\n"
		"mcr	p15, 0, r0, c8, c6, 0;  @ invalidate D TLBs\n"
		"mcr	p15, 0, r0, c8, c5, 0;  @ invalidate I TLBs\n"
		:
		:
		: "r0"
	);
}

static void mmu_map_l2_page(virtual_addr_t virt, virtual_size_t size, physical_addr_t phys, u32_t attr)
{
	u32_t * pte;
	size_t npte = size >> 10;
	int tstart = virt >> 20;
	int tend = (virt + size) >> 20;
	int i, n;

	if((phys & (SZ_1M - 1)) || (size & (SZ_1M - 1)))
		return;

	pte = memalign(PAGE_SIZE, npte * sizeof(u32_t));
	if(!pte)
		return;

	attr &= (0x3 << 2);
	for(i = 0; i < npte; i++)
	{
		pte[i] = (phys + i * PAGE_SIZE) | PTE_TYPE_SMALL | attr;
	}

	n = 0;
	for(i = tstart; i < tend; i++)
	{
		__mmu_ttb[i] = (unsigned long)(&pte[n]) | (1 << 4) | PMD_TYPE_COARSE;
		n += 256;
	}

	__dma_flush_range((unsigned long)__mmu_ttb, (unsigned long)__mmu_ttb + sizeof(__mmu_ttb));
	__dma_flush_range((unsigned long)pte, npte * sizeof(u32_t));

	tlb_invalidate();
}

static u32_t * mmu_search_pte(virtual_addr_t virt)
{
	u32_t * pte;

	if((__mmu_ttb[virt >> 20] & 0x3) != PMD_TYPE_COARSE)
		return 0;

	pte = (u32_t *)(__mmu_ttb[virt >> 20] & ~0x3ff);
	return &pte[(virt >> PAGE_SHIFT) & 0xff];
}

static void mmu_remap_range(virtual_addr_t virt, virtual_size_t size, u32_t attr)
{
	u32_t * pte;
	int i, n;

	n = size >> PAGE_SHIFT;
	pte = mmu_search_pte(virt);

	attr &= (0x3 << 2);
	for(i = 0; i < n; i++)
	{
		pte[i] &= ~((1 << PAGE_SHIFT) - 1);
		pte[i] |= PTE_TYPE_SMALL | attr;
	}

	__dma_flush_range((unsigned long)pte, (unsigned long)pte + n * sizeof(u32_t));
	tlb_invalidate();
}

void mmu_setup(void)
{
	__mmu_cache_invalidate();

	ttb_set((u32_t)(__mmu_ttb));
	domain_set(0x3);

	mmu_map_l1_section(0x00000000, SZ_2G, 0x00000000, PMD_NCNB);
	mmu_map_l1_section(0x80000000, SZ_2G, 0x80000000, PMD_NCNB);

	mmu_map_l2_page(0x30000000, SZ_512M, 0x30000000, PTE_CB);

	__mmu_cache_on();
	__mmu_cache_flush();

	icache_enable();
	dcache_enable();
	wbuffer_enable();
	branch_enable();

	mmu_remap_range(0x30000000, SZ_512M, PTE_CB);
}

physical_addr_t virt_to_phys(virtual_addr_t virt)
{
	return (physical_addr_t)virt;
}

virtual_addr_t phys_to_virt(physical_addr_t phys)
{
	return (virtual_addr_t)phys;
}

void * dma_alloc_coherent(size_t size)
{
	void * ret;

	size = PAGE_ALIGN(size);
	ret = memalign(PAGE_SIZE, size);

	__dma_inv_range((unsigned long)ret, (unsigned long)ret + size);
	mmu_remap_range((virtual_addr_t)ret, size, PTE_NCNB);

	return ret;
}

void dma_free_coherent(void * mem, size_t size)
{
	size = PAGE_ALIGN(size);
	mmu_remap_range((virtual_addr_t)mem, size, PTE_CB);
	free(mem);
}