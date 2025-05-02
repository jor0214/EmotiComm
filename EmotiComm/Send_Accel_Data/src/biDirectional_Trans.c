#include "biDirectional_Trans.h"
#include "stm32l4xx.h" 

void enable_Transmit(int RE,int DE)
{
    GPIOB->ODR |= (1 << RE); //RE = 1 disable receiver 
    GPIOB->ODR |= (1 << DE); //DE = 1 enable transmission
}
void enable_Recieve(int RE,int DE)
{
    GPIOB->ODR &= ~(1 << RE);     // RE = 0 enable receiver
    GPIOB->ODR &= ~(1 << DE);   // DE = 0 disable transmission
}


