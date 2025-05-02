#include <stm32l432xx.h>
#define READ 1
#define WRITE 0
void ResetI2C();
void I2CStart(uint8_t address, int rw, int nbytes);
void I2CReStart(uint8_t address, int rw, int nbytes);
void I2CStop();
void I2CWrite(uint8_t Data);
void delay(volatile uint32_t dly);
uint8_t I2CRead();