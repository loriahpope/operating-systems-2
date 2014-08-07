#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "mmu.h"
#include "page.h"
#include "cpu.h"

/* The following machine parameters are being used:
   
   Number of bits in an address:  32
   Page size: 4KB

   Page Table Type:  2 level page table
   Size of first level page table: 1024 entries
   Size of first level Page Table Entry:  32 bits
   Size of each 2nd Level Page Table: 1024 entries
   Size of 2nd Level Page Table Entry: 32 bits

   Bits of address giving index into first level page table: 10
   Bits of address giving index into second level page table: 10
   Bits of address giving offset into page: 12 
*/

/* Each entry of a 2nd level page table has
   the following:
     Present/Absent bit: 1 bit
     Page Frame: 20 bits 
*/

/* Type definition for an entry in a second level page table */
typedef unsigned int PT_ENTRY;

/* Present bit is the most significant bit in the page table entry */
#define PRESENT_BIT_MASK   0x80000000  

/* Lowest 20 bits of word */
#define PFRAME_MASK 	   0x000FFFFF 

/* Declare the first level page table */
PT_ENTRY **first_level_page_table;

/* Size of each page table */
#define PT_SIZE 1024

/* Sets up the initial PT. Also sets all entries of the first level PT to NULL. When a new page is referenced by the CPU the second level PT to store that entry is created if it doesn't already exist */
void pt_initialize_page_table()
{
  int i;
  first_level_page_table = (PT_ENTRY **) malloc(PT_SIZE * sizeof(PT_ENTRY *));
  
  for(i = 0; i < PT_SIZE; i++){
    first_level_page_table[i] = NULL;
  }
}

/* indexes into the first PT by performing DIV by 1024 (2^10) */
#define DIV_FIRST_PT_SHIFT 10  

/* indexes into the second PT by performing MOD 1024 (2^10) */
#define MOD_SECOND_PT_MASK 0x3FF 

/* true if there is a page fault */
BOOL page_fault;

/* Called when there is a TLB_miss. Looks up the page frame for the corresponding virtual page using the PT. If the desired bit is not present, there is a page_fault. */
PAGEFRAME_NUMBER pt_get_pageframe(VPAGE_NUMBER vpage)
{
  int first_level_pt = vpage >> DIV_FIRST_PT_SHIFT;
  int second_level_pt = vpage & MOD_SECOND_PT_MASK;
  PT_ENTRY *second_pt_entries = first_level_page_table[first_level_pt];
  
  if(second_pt_entries == NULL){	
    page_fault = TRUE;
  }
  else{
    BOOL present_bit = second_pt_entries[second_level_pt] & PRESENT_BIT_MASK;
    if(present_bit){
      page_fault = FALSE;
      return second_pt_entries[second_level_pt] & PFRAME_MASK;
    }
    else
      page_fault = TRUE;
  }
  return 0;
}

/* Inserts an entry mapping of the specified virtual page to the specified page frame into the PT. May require the creation of a second-level PT to hold the entry if it doesn't already exist */
void pt_update_pagetable(VPAGE_NUMBER vpage, PAGEFRAME_NUMBER pframe)
{	
  int first_level_pt = vpage >> DIV_FIRST_PT_SHIFT;
  int second_level_pt = vpage & MOD_SECOND_PT_MASK;
  
  if(first_level_page_table[first_level_pt] == NULL){
    first_level_page_table[first_level_pt] = (PT_ENTRY *) malloc(PT_SIZE * sizeof(PT_ENTRY));				
  }
  
  //set the present bit for the new entry
  first_level_page_table[first_level_pt][second_level_pt] = PRESENT_BIT_MASK | (pframe & PFRAME_MASK);	
}

/* Clears a PT entry by clearing its present bit. Called by the OS in (kernel.c) when a page is evicted from a page frame. */
void pt_clear_page_table_entry(VPAGE_NUMBER vpage)
{
  int first_level_pt = vpage >> DIV_FIRST_PT_SHIFT;
  int second_level_pt = vpage & MOD_SECOND_PT_MASK;
  PT_ENTRY *second_pt_entries = first_level_page_table[first_level_pt];
  
  if(second_pt_entries != NULL){
    second_pt_entries[second_level_pt] = second_pt_entries[second_level_pt] & (~PRESENT_BIT_MASK);
  }
}


