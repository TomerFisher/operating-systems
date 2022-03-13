
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "os.h"

#define NUM_OF_LEVELS 5
#define BITS_PER_LEVEL 9
#define OFFSET_SIZE 12
#define VALID_BIT_MASK 0x1
#define VPN_PART_MASK 0x1ff

/*
Number of page table levels: 
The virtual address size of our hardware is 64 bits, of which only the lower 57 bits are used for translation.
Bits 0–11 are the offset, so that leaves us 45 bits. 9 bit per level => 5-level page table.
*/

/*
 * Function: page_table_update
 * --------------------
 * create/destroy virtual memory mappings in a page table
 *
 *  pt: the physical page number of the page table root
 *  vpn: the virtual page number the caller wishes to map/unmap
 *  ppn: the physical page number that vpn should be mapped to
 *       if equal to a special NO MAPPING value, then vpn’s mapping should be destroyed
 *
 *  returns: nothing
 */

void page_table_update_helper(uint64_t* pt, uint64_t vpn, uint64_t ppn, int level)
{
	uint64_t vpn_part = (vpn >> (BITS_PER_LEVEL*(NUM_OF_LEVELS-1-level))) & VPN_PART_MASK; // get the VPN part in this spesific level
	if (level == NUM_OF_LEVELS - 1) { // reache to the last table page
		if (ppn == NO_MAPPING) {
			pt[vpn_part] = 0x0; // destroy vpn's mapping
			// TODO: create function to free pages if needed
			return;
		}
		pt[vpn_part] = (ppn << OFFSET_SIZE) + VALID_BIT_MASK; // create vpn's mapping
		return;
	}
	uint64_t pte = pt[vpn_part]; // get the pte by the vpn_part
	if ((pte & VALID_BIT_MASK) == 0) {  // check if pte is valid
		if (ppn == NO_MAPPING)
			return; // pte is not valid and ppn equal to NO_MAPPING, nothing to do
		pte = (alloc_page_frame() << OFFSET_SIZE) + VALID_BIT_MASK; // allocate new page frame for pte and make it valid
		pt[vpn_part] = pte; // set the new pte in the page table
	}
	pt = phys_to_virt(pte - VALID_BIT_MASK); // get va for the next page table (of level+1)
	page_table_update_helper(pt, vpn, ppn, level + 1);
}

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn)
{
	uint64_t* table_root = phys_to_virt(pt << OFFSET_SIZE);
	page_table_update_helper(table_root, vpn, ppn, 0);
}

/*
 * Function: page_table_query
 * --------------------
 * query the mapping of a virtual page number in a page table
 *
 *  pt: the physical page number of the page table root
 *  vpn: the virtual page number
 *
 *  returns: the physical page number that vpn is mapped to, or NO MAPPING if no mapping exists
 */

uint64_t page_table_query_helper(uint64_t* pt, uint64_t vpn, int level)
{
	uint64_t vpn_part = (vpn >> (BITS_PER_LEVEL*(NUM_OF_LEVELS-1-level))) & VPN_PART_MASK; // get the VPN part in this spesific level
	uint64_t pte = pt[vpn_part]; // get the pte by the vpn_part
	if ((pte & VALID_BIT_MASK) == 0) // check if pte is valid
		return NO_MAPPING;
	if (level == NUM_OF_LEVELS - 1) // reache to the last table page
		return pte >> OFFSET_SIZE;
	pt = phys_to_virt(pte - VALID_BIT_MASK); // get va for the next page table (of level+1)
	return page_table_query_helper(pt, vpn, level+1);
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn)
{
	uint64_t* table_root = phys_to_virt(pt << OFFSET_SIZE);
	return page_table_query_helper(table_root, vpn, 0);
}
