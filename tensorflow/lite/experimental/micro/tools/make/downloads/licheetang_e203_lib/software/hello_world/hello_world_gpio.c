// See LICENSE for license details.

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include <string.h>
#include "plic/plic_driver.h"
#include "encoding.h"
#include <unistd.h>
#include "stdatomic.h"

// Structures for registering different interrupt handlers
// for different parts of the application.
typedef void (*function_ptr_t) (void);

void no_interrupt_handler (void) {};

function_ptr_t g_ext_interrupt_handlers[PLIC_NUM_INTERRUPTS];


// Instance data for the PLIC.

plic_instance_t g_plic;


volatile uint32_t delay_ms_cnt = 0;

/*Entry Point for PLIC Interrupt Handler*/
void handle_m_ext_interrupt(){
  plic_source int_num  = PLIC_claim_interrupt(&g_plic);
  if ((int_num >=1 ) && (int_num < PLIC_NUM_INTERRUPTS)) {
    g_ext_interrupt_handlers[int_num]();
  }
  else {
    exit(1 + (uintptr_t) int_num);
  }
  PLIC_complete_interrupt(&g_plic, int_num);
}


/*Entry Point for Machine Timer Interrupt Handler*/
void handle_m_time_interrupt(){

  clear_csr(mie, MIP_MTIP);

  // Reset the timer for 1ms in the future.
  // This also clears the existing timer interrupt.
  volatile uint64_t * mtime       = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIME);
  volatile uint64_t * mtimecmp    = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
  uint64_t now = *mtime;
  uint64_t then = now + (RTC_FREQ/1000);
  *mtimecmp = then;

  delay_ms_cnt++;
  
  // Re-enable the timer interrupt.
  set_csr(mie, MIP_MTIP);

}

static void delay_ms(uint32_t delay)
{
  uint32_t now = 0;
  now = delay_ms_cnt + delay;
  while(now > delay_ms_cnt);
}

void reset_demo ()
{
  // Disable the machine & timer interrupts until setup is done.
  clear_csr(mie, MIP_MEIP);
  clear_csr(mie, MIP_MTIP);

  for (int ii = 0; ii < PLIC_NUM_INTERRUPTS; ii ++){
    g_ext_interrupt_handlers[ii] = no_interrupt_handler;
  }

    // Set the machine timer to go off in 1 millisecond.
    volatile uint64_t * mtime       = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIME);
    volatile uint64_t * mtimecmp    = (uint64_t*) (CLINT_CTRL_ADDR + CLINT_MTIMECMP);
    uint64_t now = *mtime;
    uint64_t then = now + (RTC_FREQ/1000);
    *mtimecmp = then;

    // Enable the Machine-External bit in MIE
    set_csr(mie, MIP_MEIP);

    // Enable the Machine-Timer bit in MIE
    set_csr(mie, MIP_MTIP);

    // Enable interrupts in general.
    set_csr(mstatus, MSTATUS_MIE);
}

int main(int argc, char **argv)
{
  GPIO_REG(GPIO_INPUT_EN) &= ~((0x1<< 1) | (0x1<< 2) | (0x1 << 3));
  GPIO_REG(GPIO_OUTPUT_EN) |= ((0x1<< 1) | (0x1<< 2) | (0x1 << 3));
  GPIO_REG(GPIO_OUTPUT_VAL) |= ((0x1<< 1) | (0x1<< 2) | (0x1 << 3));
  GPIO_REG(GPIO_OUTPUT_VAL) &= ~((0x1<< 1) | (0x1<< 2) | (0x1 << 3));
  /**************************************************************************
   * Set up the PLIC
   *
   *************************************************************************/
  PLIC_init(&g_plic,
	    PLIC_CTRL_ADDR,
	    PLIC_NUM_INTERRUPTS,
	    PLIC_NUM_PRIORITIES);


  reset_demo();
  int test = 0;

  while(1)
  {
    printf("Hello world from Lichee Tang!\n");

    // read the current value of the LEDS and invert them.
    uint32_t leds = GPIO_REG(GPIO_OUTPUT_VAL);

    GPIO_REG(GPIO_OUTPUT_VAL) ^= ((0x1 << 1));
    delay_ms(300);
    GPIO_REG(GPIO_OUTPUT_VAL) ^= ((0x1 << 2));
    delay_ms(300);
    GPIO_REG(GPIO_OUTPUT_VAL) ^= ((0x1 << 3));
    delay_ms(300);
  }
  return 0;

}
