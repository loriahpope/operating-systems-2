#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "tlb.h"
#include "cpu.h"
#include "mmu.h"

/* I defined the TLB as an array of entries,
   each containing the following:
   Valid bit: 1 bit
   Virtual Page: 20 bits
   Modified bit: 1 bit
   Reference bit: 1 bit
   Page Frame: 20 bits (of which only 18 are meaningful given 1GB RAM)
*/

/*You can use a struct to get a two-word entry.*/
typedef struct {
  unsigned int vbit_and_vpage;  // 32 bits containing the valid bit and the 20bit
                                // virtual page number.
  unsigned int mr_pframe;       // 32 bits containing the modified bit, reference bit,
                                // and 20-bit page frame number
} TLB_ENTRY;


/*Dynamic TLB array allocated to the right size based on num_tlb_entries*/
TLB_ENTRY *tlb;  


/*Size of the TLB chosen by the user*/
unsigned int num_tlb_entries;

/*this must be set to TRUE when there is a tlb miss, FALSE otherwise.*/
BOOL tlb_miss; 


/*masks used to select the various fields of a TLB entry.*/
#define VBIT_MASK   0x80000000  //VBIT is leftmost bit of first word
#define VPAGE_MASK  0x000FFFFF            //lowest 20 bits of first word
#define RBIT_MASK   0x80000000  //RIT is leftmost bit of second word
#define MBIT_MASK   0x40000000  //MBIT is second leftmost bit of second word
#define PFRAME_MASK 0x000FFFFF            //lowest 20 bits of second word


/*Initialize the TLB (called by the mmu)*/
void tlb_initialize()
{
  /*Here's how you can allocate a TLB of the right size*/
  tlb = (TLB_ENTRY *) malloc(num_tlb_entries * sizeof(TLB_ENTRY));

  /*initialize each tlb entry by clearing the valid bit*/
  tlb_clear_all();
}

/*clears the entire TLB by clearing the valid bit for every entry.*/
void tlb_clear_all() 
{
  int i = 0;
  for(i; i < num_tlb_entries; i++){
    //set v bit to 0 by using an and/not operation
    tlb[i].vbit_and_vpage = tlb[i].vbit_and_vpage & ~VBIT_MASK;
  }
}

/*This clears all the R bits in the TLB*/
void tlb_clear_all_R_bits() 
{
  int i = 0;
  for(i; i < num_tlb_entries; i++){
    //set r bit to 0 by using an and/not operation
    tlb[i].mr_pframe = tlb[i].mr_pframe & ~RBIT_MASK;
  }
}

/*This clears out the entry in the TLB for the specified virtual page, by clearing the valid bit for that entry.*/
void tlb_clear_entry(VPAGE_NUMBER vpage) {
  int i = 0;
  for(i; i< num_tlb_entries; i++){
    if((tlb[i].vbit_and_vpage & VPAGE_MASK) == vpage)
      {
	//set v bit to 0 by using an and/not operation
	tlb[i].vbit_and_vpage = tlb[i].vbit_and_vpage & ~VBIT_MASK;
      }
  }
}

/*Returns a page frame number if there is a TLB hit. If there is a TLB miss, then it sets tlb_miss (see above) to TRUE.  It sets the R bit of the entry and, if the specified operation is a STORE, sets the M bit. */
PAGEFRAME_NUMBER tlb_lookup(VPAGE_NUMBER vpage, OPERATION op)
{
  int i;
  for(i = 0; i < num_tlb_entries; i++){
    //page hit
    if((tlb[i].vbit_and_vpage & VPAGE_MASK) == vpage && ((tlb[i].vbit_and_vpage & VBIT_MASK) != 0)){
      tlb_miss = FALSE;
      //set r bit
      tlb[i].mr_pframe = tlb[i].mr_pframe | RBIT_MASK;
      //set m bit if store operation
      if (op == STORE){
	tlb[i].mr_pframe = tlb[i].mr_pframe | MBIT_MASK; 
      }
      //return corresponding page frame
      return tlb[i].mr_pframe & PFRAME_MASK;
    }
    //page miss
    tlb_miss = TRUE;
  }
}

/*Uses an NRU clock algorithm, where the first entry with either a cleared valid bit or cleared R bit is chosen.
  points to next TLB entry to consider evicting*/
int clock_hand = 0;  

/*Starting at the clock_hand'th entry, find first entry to evice with either valid bit = 0 or the R bit = 0. If there 
  is no such entry, then just evict the entry pointed to by the clock hand. */
void tlb_insert(VPAGE_NUMBER new_vpage, PAGEFRAME_NUMBER new_pframe,
		BOOL new_mbit, BOOL new_rbit) {
  int i; 
  for (i = clock_hand; i < num_tlb_entries; i++){
    if(tlb[i].vbit_and_vpage & VBIT_MASK == 0 || tlb[i].mr_pframe & RBIT_MASK == 0){
      clock_hand = i;
      break;
    }
  }
  
  //if entry is valid, write M & R bits of entry back to M & R bitmaps
  if((tlb[clock_hand].vbit_and_vpage & VBIT_MASK) != 0){
    PAGEFRAME_NUMBER pageframe = tlb[clock_hand].mr_pframe & PFRAME_MASK;
    int r = tlb[clock_hand].mr_pframe & RBIT_MASK;
    int m = tlb[clock_hand].mr_pframe & MBIT_MASK;
    mmu_modify_rbit_bitmap(pageframe, r);
    mmu_modify_mbit_bitmap(pageframe, m);
  }
  
  //insert new vpage into found/evicted TLB entry
  (tlb[clock_hand]).vbit_and_vpage = tlb[clock_hand].vbit_and_vpage | VBIT_MASK;
  (tlb[clock_hand]).vbit_and_vpage = tlb[clock_hand].vbit_and_vpage | VPAGE_MASK;
  (tlb[clock_hand]).vbit_and_vpage = tlb[clock_hand].vbit_and_vpage & (new_vpage | ~VPAGE_MASK);
  
  //insert new mbit into found/evicted TLB entry
  (tlb[clock_hand]).mr_pframe = tlb[clock_hand].mr_pframe | MBIT_MASK;
  (tlb[clock_hand]).mr_pframe = tlb[clock_hand].mr_pframe & ((new_mbit << 31) | ~MBIT_MASK);
  
  //insert new rbit into found/evicted TLB entry
  (tlb[clock_hand]).mr_pframe = tlb[clock_hand].mr_pframe | RBIT_MASK;
  (tlb[clock_hand]).mr_pframe = tlb[clock_hand].mr_pframe & ((new_rbit << 30) | ~RBIT_MASK);
  
  //insert new pageframe into found/evicted TLB entry
  (tlb[clock_hand]).mr_pframe = tlb[clock_hand].mr_pframe | PFRAME_MASK;
  (tlb[clock_hand]).mr_pframe = tlb[clock_hand].mr_pframe & (new_pframe | ~PFRAME_MASK);
  
  //advance the clock hand
  clock_hand++;
  
  //if there are no more entries set the clock hand to 0 (to avoid segmentation fault)
  if(clock_hand > num_tlb_entries - 1){
    clock_hand = 0;
  }
}


/*Writes the M  & R bits in the each valid TLB entry back to the M & R MMU bitmaps.*/
void tlb_write_back()
{
  int i;
  for(i = 0; i< num_tlb_entries; i++){
    if((tlb[i].vbit_and_vpage & VBIT_MASK) != 0){
      PAGEFRAME_NUMBER pageframe = tlb[i].mr_pframe & PFRAME_MASK;
      int r = tlb[i].mr_pframe & RBIT_MASK;
      int m = tlb[i].mr_pframe & MBIT_MASK;
      mmu_modify_rbit_bitmap(pageframe, r);
      mmu_modify_mbit_bitmap(pageframe, m);
    }
  }
}

