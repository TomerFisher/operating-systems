
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
 * Function: check_if_mapping_exists
 * --------------------
 * check if all the mappings in the page table node are NO MAPPING
 *
 *  pt_node: the va of page table node
 *
 *  returns: 1 if all the mappings are NO MAPPING, otherwise (there is at least one mapping) 0
 */

uint64_t check_if_mapping_exists (uint64_t* pt_node)
{
	uint64_t pte;
	for(uint64_t vpn_part=0x0; vpn_part <= VPN_PART_MASK; vpn_part++){
		pte = pt_node[vpn_part];
		if ((pte & VALID_BIT_MASK) == 1)
			return 0;
	}
	return 1;
}

/*
 * Function: free_page_table_nodes
 * --------------------
 * free page table nodes in recursion on the levels
 *
 *  pt_node: the va of the page table at the current level
 *  vpn: the virtual page number the caller unmaped (in page_table_update_helper)
 *  level: the level
 *
 *  returns: nothing
 */

void free_page_table_nodes(uint64_t* pt_node, uint64_t vpn, int level)
{
	uint64_t vpn_part, pte, ppn, next_vpn_part, next_pte;
	uint64_t* next_pt_node;
	uint64_t* first_pt_node = pt_node;
	if (level == 1)
		return;
	for(int index=0; index < level - 2; index++) {
		vpn_part = (vpn >> (BITS_PER_LEVEL*(NUM_OF_LEVELS-1-index))) & VPN_PART_MASK; // get the VPN part in this spesific level
		pte = pt_node[vpn_part]; // get the pte by the vpn_part
		pt_node = phys_to_virt(pte - VALID_BIT_MASK); // get va for the page table node (of level+1)
	}
	next_vpn_part = (vpn >> (BITS_PER_LEVEL*(NUM_OF_LEVELS-level+1))) & VPN_PART_MASK; // get the next VPN part in this spesific level
	next_pte = pt_node[next_vpn_part]; // get the next pte by next_vpn_part
	next_pt_node = phys_to_virt(next_pte - VALID_BIT_MASK); // get va for the next page table node
	if (check_if_mapping_exists(next_pt_node)) { // if all the mappings of the next page table node are NO MAPPING
		ppn = next_pte >> OFFSET_SIZE;
		free_page_frame(ppn); // free page table node
		pt_node[next_vpn_part] = 0x0; // destroy parent mapping
		free_page_table_nodes(first_pt_node, vpn, level - 1);
	}
}

/*
 * Function: page_table_update_helper
 * --------------------
 * create/destroy virtual memory mappings in a page table in recursion on the levels
 *
 *  pt_node: the va of the page table at the current level
 *  vpn: the virtual page number the caller wishes to map/unmap
 *  ppn: the physical page number that vpn should be mapped to
 *       if equal to a special NO MAPPING value, then vpn’s mapping should be destroyed
 *  level: the level
 *
 *  returns: 1 if destroy mapping, otherwise 0
 */

uint64_t page_table_update_helper(uint64_t* pt_node, uint64_t vpn, uint64_t ppn, int level)
{
	uint64_t vpn_part = (vpn >> (BITS_PER_LEVEL*(NUM_OF_LEVELS-1-level))) & VPN_PART_MASK; // get the VPN part in this spesific level
	if (level == NUM_OF_LEVELS - 1) { // reache to the last table page
		if (ppn == NO_MAPPING) {
			pt_node[vpn_part] = 0x0; // destroy vpn's mapping
			return 1;
		}
		pt_node[vpn_part] = (ppn << OFFSET_SIZE) + VALID_BIT_MASK; // create vpn's mapping
		return 0;
	}
	uint64_t pte = pt_node[vpn_part]; // get the pte by the vpn_part
	if ((pte & VALID_BIT_MASK) == 0) {  // check if pte is valid
		if (ppn == NO_MAPPING)
			return 0; // pte is not valid and ppn equal to NO_MAPPING, nothing to do
		pte = (alloc_page_frame() << OFFSET_SIZE) + VALID_BIT_MASK; // allocate new page frame for pte and make it valid
		pt_node[vpn_part] = pte; // set the new pte in the page table
	}
	pt_node = phys_to_virt(pte - VALID_BIT_MASK); // get va for the next page table (of level+1)
	return page_table_update_helper(pt_node, vpn, ppn, level + 1);
}

/*
 * Function: page_table_update
 * --------------------
 * create/destroy virtual memory mappings in a page table, if destroy it also free page table nodes if needed
 *
 *  pt: the physical page number of the page table root
 *  vpn: the virtual page number the caller wishes to map/unmap
 *  ppn: the physical page number that vpn should be mapped to
 *       if equal to a special NO MAPPING value, then vpn’s mapping should be destroyed
 *
 *  returns: nothing
 */

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn)
{
	uint64_t* page_table_root = phys_to_virt(pt << OFFSET_SIZE);
	if (page_table_update_helper(page_table_root, vpn, ppn, 0)) 
		// the update destroy mapping so we free page table nodes if needed
		free_page_table_nodes(page_table_root, vpn, NUM_OF_LEVELS);
}

/*
 * Function: page_table_query_helper
 * --------------------
 * query the mapping of a virtual page number in a page table in recursion on the levels
 *
 *  pt_node: the va of the page table at the current level
 *  vpn: the virtual page number
 *  level: the level
 *
 *  returns: the physical page number that vpn is mapped to, or NO MAPPING if no mapping exists
 */

uint64_t page_table_query_helper(uint64_t* pt_node, uint64_t vpn, int level)
{
	uint64_t vpn_part = (vpn >> (BITS_PER_LEVEL*(NUM_OF_LEVELS-1-level))) & VPN_PART_MASK; // get the VPN part in this spesific level
	uint64_t pte = pt_node[vpn_part]; // get the pte by the vpn_part
	if ((pte & VALID_BIT_MASK) == 0) // check if pte is valid
		return NO_MAPPING;
	if (level == NUM_OF_LEVELS - 1) // reache to the last table page node
		return pte >> OFFSET_SIZE;
	pt_node = phys_to_virt(pte - VALID_BIT_MASK); // get va for the next page table node (of level+1)
	return page_table_query_helper(pt_node, vpn, level+1);
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

uint64_t page_table_query(uint64_t pt, uint64_t vpn)
{
	uint64_t* page_table_root = phys_to_virt(pt << OFFSET_SIZE);
	return page_table_query_helper(page_table_root, vpn, 0);
}

