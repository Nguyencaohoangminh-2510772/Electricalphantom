#include <clock.h>
#include <stdint.h>
#define RCC_ADDR 0x40023800
#define FLASH_ADDR 0x40023C00

void clock_init(void)
{
    uint32_t* RCC_CR=(uint32_t*)(RCC_ADDR + 0x00);
    uint32_t* RCC_PLLCFGR=(uint32_t*)(RCC_ADDR + 0x04);
    uint32_t* RCC_CFGR=(uint32_t*)(RCC_ADDR + 0x08);
    uint32_t* FLASH_ACR=(uint32_t*)(FLASH_ADDR + 0x00);

    * RCC_CR |= (1<<16); // HSE on 
    while (((* RCC_CR >> 17) & 1) ==0); // Wait flag HSE ready

    * FLASH_ACR &= ~(0b1111 <<0);
    * FLASH_ACR |= (0b0011 <<0); // Flash latency 3 WS for 100 MHz

    * RCC_PLLCFGR &= ~(0b111111 <<0); // HSE 25 MHz / 25 = 1 MHz
    * RCC_PLLCFGR |= (25 << 0);

    * RCC_PLLCFGR &= ~(0b111111111 << 6); // 1 MHz * 200 = 200 MHz
    * RCC_PLLCFGR |= (200 << 6);

    * RCC_PLLCFGR &= ~(0b11 << 16); // 200 MHz / 2 = 100 MHz SYSCLK
    * RCC_PLLCFGR |= (0 << 16);

    * RCC_PLLCFGR |= (1 << 22); // Set HSE for PLL

    * RCC_CR |= (1<<24); // PLL on 
    while (((* RCC_CR >> 25) & 1) ==0); // Wait flag PLL ready

    * RCC_CFGR &= ~(0b1111 << 4); // AHB clock not divided
    * RCC_CFGR |= (0b0000 << 4);

    * RCC_CFGR &= ~(0b111 << 10); // APB1 clock divided by 2
    * RCC_CFGR |= (0b100 << 10);

    * RCC_CFGR &= ~(0b111 << 13); // APB2 clock not divided
    * RCC_CFGR |= (0b000 << 13);
 
    * RCC_CFGR &= ~(0b11 << 0);
    * RCC_CFGR |= (0b10 <<0); // PLL -> SysClock
    while (((* RCC_CFGR >> 2) & 0b11) != 0b10);
}

void clock_enable_AHB1(AHB1_peripheral_t peripheral)
{
    uint32_t* RCC_AHB1ENR= (uint32_t*)(RCC_ADDR + 0x30);
    * RCC_AHB1ENR |= (1<< peripheral);
}

void clock_enable_AHB2(AHB2_peripheral_t peripheral)
{
    uint32_t* RCC_AHB2ENR= (uint32_t*)(RCC_ADDR + 0x34);
    * RCC_AHB2ENR |= (1<< peripheral);
}

void clock_enable_APB1(APB1_peripheral_t peripheral)
{
    uint32_t* RCC_APB1ENR= (uint32_t*)(RCC_ADDR + 0x40);
    * RCC_APB1ENR |= (1<< peripheral);
}

void clock_enable_APB2(APB2_peripheral_t peripheral)
{
    uint32_t* RCC_APB2ENR= (uint32_t*)(RCC_ADDR + 0x44);
    * RCC_APB2ENR |= (1<< peripheral);
}
