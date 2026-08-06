#include "clock.h"
#include <stdint.h>

#define TIM1_ADDR_BASE 0x40010000
#define ADD_NVIC 0xE000E100
#define TIMER1 1
#define SYSTICK 2
#define DELAY_SRC TIMER1 

void delay_init(void){

   #if (DELAY_SRC == TIMER1)   
      clock_enable_APB2(TIM1_peripheral);
  // set 1 msec for timer1
  // rcc --> 100Mhz --> PSC = 100 --> 1000000Hz | 1cnt - 1us
  //ARR = 1000    |  1000cnt - 1ms 
 
 uint16_t* TIM1_ARR = (uint16_t*) (TIM1_ADDR_BASE + 0x2C);
 uint16_t* TIM1_PSC = (uint16_t*) (TIM1_ADDR_BASE + 0x28);
 *TIM1_PSC = 100-1;
 *TIM1_ARR = 1000-1;
 //interrupt timer1
  uint16_t* TIM1_DIER = (uint16_t*) (TIM1_ADDR_BASE + 0x0C);
    *TIM1_DIER |= ( 1 << 0 ); // enable interrupt for timer1

uint16_t* TIM1_CR1 = (uint16_t*) (TIM1_ADDR_BASE + 0x00);
*TIM1_CR1 |= ( 1 << 0 ); // enable counter for timer1

uint32_t* NVIC_ISER0 = (uint32_t*) (ADD_NVIC + 0x00);
 *NVIC_ISER0 |= (1 << 25 );  // enable timer1 interrupt in NVIC
#else // delay using systick
      uint32_t* SYST_CSR = (uint32_t*) (0xE000E010);
        *SYST_CSR |= (0b11 << 0); // enable systick and interrupt systick

      uint32_t* SYST_RVR = (uint32_t*) (0xE000E014);
        *SYST_RVR = 16000 - 1; // 1ms
  // systick is in the <0 position> of the NVIC, so we don't need to enable it in the NVIC 
    

#endif

}

volatile uint32_t tim1_cnt = 0;


#if (DELAY_SRC == TIMER1) 
void TIM1_UP_TIM10_IRQHandler(){
   uint16_t* TIM1_SR = (uint16_t*) (TIM1_ADDR_BASE + 0x10);
      *TIM1_SR &= ~( 1 << 0 ); // clear UIF flag 
      tim1_cnt++;
}
#else // delay using systick
//void SysTick_Handler(){
  //   tim1_cnt++;
//}
#endif

void delay_ms(uint32_t ms){
        uint32_t current_time = tim1_cnt;
        while ((tim1_cnt - current_time) < ms);
}




