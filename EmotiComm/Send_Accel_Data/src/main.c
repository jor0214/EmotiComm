/* 
Project : EMOTICOMM - SENSOR DATA SENDER BOARD 
Jennifer O'reilly - C21475355, Claire Murphy - C21337056 , 
Last Updated : 30/04/2025

- This code runs on the sending STM32 Nucleo L432KC board in a bidirectional embedded system
 communication setup. 
- The board continuously reads X, Y, and Z accelerometer data using I2C and transmits this data 
  to a paired receiver board over UART (USART1).
- Each data packet is formatted with square bracket delimiters ('[' and ']') for reliable parsing.
- After sending a message, the board waits for an [Ack] response from the receiver to validate
  correct switching of transceiver direction 
- If no acknowledgement is received, the user can press a physical button to manually resend the message.
- The board supports a "Pong Mode," toggled by an interrupt-driven button. In Pong Mode : 
    - "PONG" message is sent to reciever board to tell it to stop sending acks
    - accelerometer values are sent in a loop to enable paddle control on the receiving board.
    - sender stops waiting for acknowledgements during Pong Mode to allow real-time motion input.
    - "EXIT" message is sent to reciver baord to exit pong mode when button is pressed
- USART2 is used to output debug and validation messages to a serial monitor.
- LCD output provides visual feedback for Ack reciever verification and pong mode verification.

Core components include:
- I2C interface for reading accelerometer data
- USART1 for board-to-board UART communication
- USART2 for serial debugging
- GPIO control of a bidirectional transceiver (RE/DE pins)
- Interrupt-driven button handling for mode switching
- LCD feedback display via SPI
- Circular buffer and message parsing with acknowledgment logic
*/

//include necessary header files
#include <eeng1030_lib.h>
#include <stdio.h>
#include  <errno.h>
#include  <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include "circular_buffer.h"
#include "display.h"
#include "biDirectional_Trans.h"
#include <string.h>
#include "i2c.h"         //For reading accelerometer data 
#include "spi.h"        // For SPI communication with LCD
#include "display.h"   // For LCD drawing functions like printText()


                                                  //assembly instruction functions embedded with c to enable/disable interrupts
#define enable_interrupts() asm(" cpsie i ")     //Change Processor State Interrupt Enable
#define disable_interrupt() asm (" cpsid i")    //Change Processor State Interrupt Disable
#define INPUT_BUFFER_SIZE CIRC_BUF_SIZE        //Input buffer size equal to circular buffer for consitency  
#define LINE_HEIGHT 10                        //LCD dimension definitions 
#define SCREEN_HEIGHT 80
#define SCREEN_WIDTH 160
#define NUM_LINES 8

//function prototypes 
void setup(void);
void delay_ms(volatile uint32_t dly);
void initSerial(uint32_t baudrate);
void eputc(char c);
void shiftdisp(int i,const char *message);
void sendMessage();
void clearMessage(char *buffer, int size);
void printMessage(int i,const char *message);
void shiftdisp(int type,const char *message);
int buttonpressed(int buttonpin);
void sendPongMessage(int pongmode);
void measureAccel();

//variables declarations 
int count;
int currentLine = 0;
circular_buffer rx_buf;                             //Circular buffer structure defined in circular buffer.h
volatile uint32_t data_ready=0;                    //flag to indicate when meesage is finished being added to the buffer
volatile char input_buffer[INPUT_BUFFER_SIZE];    //stores user-typed message from serial port
volatile uint8_t input_index = 0;                //index variable to irerate through characters in input message
volatile uint8_t input_ready = 0;               //flag to indicate when input message is ready to be sent 
static volatile uint8_t message_started = 0;   //Flag to indicate when message has started being recieved 
char messagesdisp[8][24];                     //stores ch line of messages to be printed to LCD
int messagetype[8];                          //stores whether message is recieved message or sent message to detemine how it is displayed on LCD
int ack_recieved = 0; 
volatile int pongMode = 0;
// The following timeout value was determined by
// trial and error running the chip at 72MHz
// There is a little bit of 'headroom' included
#define I2C_TIMEOUT 20000
uint16_t response;
int16_t x_accel;
int16_t y_accel;
int16_t z_accel;
int32_t X_g;
int32_t Y_g;
int32_t Z_g;

int main()
{

    char message_received[CIRC_BUF_SIZE];      //stores recieved message from circular buffer
    int currentButton = 0;
    int previousButton = 0;
    setup();  
    initI2C();         //setup i2c peripheral
    ResetI2C();      
    // Take accelerometer out of power-down mode
    I2CStart(0x69, WRITE, 2);
    I2CWrite(0x7E);   
    I2CWrite(0x11);         // Normal mode
    I2CStop();
    delay_ms(1000000);     // Wait for startup                    
    init_display();
    init_circ_buf(&rx_buf);
    NVIC->ISER[1] |= (1 << (37-32));        //ensures that the USART1_IRQHandler() gets triggered when data is received via UART2
    enable_interrupts();
    while(1)
    {

    //pong mode is determined by push button detected by interupt
    if (pongMode == 1)
   {
        //pong mode set up
        enable_Transmit(4,5);                                     //enable transmitter 
        printf("PONG MODE - PRESS BUTTON TO EXIT...\r\n");              
        printMessage(0,"PONG MODE = PRESS BUTTON TO EXIT");
        sendPongMessage(pongMode);                             //call pong message function - to alert other board that this board is in pong mode  
        delay(10000);
        
        while(pongMode)
        {
            //pong mode involves measuring acceleration and sending the acceleration values in a continous loop 
            measureAccel();
            sendMessage();
            delay(1000);  
        }
        printf("EXITING PONG MODE..\r\n");
        printMessage(0,"EXITING PONG MODE");
        
    }
        //Outside of pong mode, operation involves measuring accel data,sending accel data, waiting for ack, repeating when ack is recieved
        measureAccel();
        sendMessage();
        enable_Recieve(4,5);
        ack_recieved = 0;
        printf("waiting for ack.....");

    //loop used while waiting for ack to be recieved from other board
        while(ack_recieved == 0)
    {
 
        if (data_ready)                                     //data ready flag controller by usart1 interupt handler
        {
            
            int i = 0;                                  
            char c;                                       
            while(get_circ_buf(&rx_buf, &c) == 0)         //get circ buffer fuction returns 0 when circular buffer is not empty    
            {
                message_received[i++] = c;              //next character of buffer is retrieved and stored in message array each time get_circ_buf function is called
            }
            message_received[i+1] = '\0';  
           printf("Message received from USART1: %s\n", message_received);      //print to serial monitor (usart2) 
          //  printf("\rReceived message: %s\r\n", message_received);


          //if message recieved from recieving board is an Ack message
            if (strcmp(message_received, "Ack") == 0)
            {
                printf("ACK RECIEVED\r\n");  
                ack_recieved = 1;              //set ack_recieved flag to allow sensor data to be read and sent again

            delay_ms(5000000);                                                 
            printMessage(1,message_received);                        //call printMessage funtion to print to LCD
            clearMessage(message_received, CIRC_BUF_SIZE);          //After message has been recieved, call function to clear message recieved function
            data_ready = 0;                                        //reset data_ready flag to zero
            }
                                         
        }
    //Check if button 1 is pressed - This button is responsable for manually sending messages after ack has not been recieved 
        currentButton = buttonpressed(0);
        if (previousButton == 0 && currentButton == 1)
        {
            //when button is pressed, the ack_recieved variable becomes one which allows the board to send on more data manually
            printf("Ack not recieved, resending message...\r\n");
            ack_recieved = 1; 
            }
            previousButton = currentButton;
        }
        
    }
}
       
void delay_ms(volatile uint32_t dly)
{
    while(dly--);
}
void setup()
{
    initClocks();    
    RCC->AHB2ENR |= (1 << 0) + (1 << 1);     // enable GPIOA and GPIOB
    pinMode(GPIOB,4,1);                     //Set pin B4 to 1 -ouput pin for RE
    pinMode(GPIOB,5,1);                    //Set pin pB5 to 1 - output pin for DE
    pinMode(GPIOB,0,0);                   //Set pin PB0 to 0 - input for push button 
    pinMode(GPIOB,1,0);                  //Set pin PB1 to 0 - inpit for push button
    enablePullUp(GPIOB,1);              //enable internal pull-up resistors for PB1
    enablePullUp(GPIOB,0);             //enable internal pull-up resistors for PB0
    initSerial(9600);
    enable_Transmit(4,5);

    //interupt setup for push button
    RCC->APB2ENR |= (1 << 0);               // Enable SYSCFG clock
    SYSCFG->EXTICR[0] &= ~(0xF << 4);      // clear EXTI1 bits
    SYSCFG->EXTICR[0] |= (1 << 4);        // Map EXTI1 to PB1
    EXTI->IMR1 |= (1 << 1);              // Unmask EXTI1
    EXTI->FTSR1 |= (1 << 1);            // Falling edge trigger (button press = low)
    EXTI->RTSR1 &= ~(1 << 1);          // No rising edge trigger
    NVIC->ISER[0] |= (1 << 7);        // Enable EXTI1 IRQ (EXTI1_IRQn = interrupt 7)

    
}
void initSerial(uint32_t baudrate)
{
    RCC->AHB2ENR |= (1 << 0);                                // make sure GPIOA is turned on
    pinMode(GPIOA,2,2);                                     // alternate function mode for PA2
    selectAlternateFunction(GPIOA,2,7);                    // AF7 = USART2 TX
    pinMode(GPIOA,15,2);                                  // alternate function mode for PA15
    selectAlternateFunction(GPIOA,15,3);                 //AF3 = USART2 RX
    RCC->APB1ENR1 |= (1 << 17);                         //turn on USART2
    
    pinMode(GPIOA,9,2);                               // alternate function mode for PA9
    selectAlternateFunction(GPIOA,9,7);              // AF7 = USART1 TX
    pinMode(GPIOA,10,2);                            // alternate function mode for P10
    selectAlternateFunction(GPIOA,10,7);           //AF7 = USART1 RX
    RCC->APB2ENR |= (1 << 14);                    // turn on USART1
    



    const uint32_t CLOCK_SPEED=80000000;    
    uint32_t BaudRateDivisor;
    
    BaudRateDivisor = CLOCK_SPEED/baudrate;      
    USART1->CR1 = 0;                                       //clear control reg 1
    USART1->CR2 = 0;                                      //clear control reg 2
    USART1->CR3 = (1 << 12);                             //disable over-run errors
    USART1->BRR = BaudRateDivisor;                      //set baud rate
    USART1->CR1 =  (1 << 3);                           //enable the transmitter 
    USART1->CR1 |=  (1 << 2);                         //enable the receiver
    USART1->CR1 |= (1 << 0);                         //enable usart1 peripheral 
    USART1->CR1 |= (1 << 5);                        //enable RXNE interrupt for USART1
    USART1->ICR = (1 << 1);                        //Clear any pending USART1 errors 
                                             
    USART2->CR1 = 0;                              //repeat for usart2
    USART2->CR2 = 0;
    USART2->CR3 = (1 << 12);             
    USART2->BRR = BaudRateDivisor;
    USART2->CR1 =  (1 << 3);   
    USART2->CR1 |=  (1 << 2);  
    USART2->CR1 |= (1 << 0);
}
int _write(int file, char *data, int len)
{
    //function is used to direct standard output such as printf(), putchar(), etc.) to the UART/USART for serial communication
    if ((file != STDOUT_FILENO) && (file != STDERR_FILENO))
    {
        errno = EBADF;                 //error is returned when file is not standard output of standard error
        return -1;
    }
    while(len--)
    {
        eputc(*data);                 //data string is iterated through and eputc function is called to send each character over USART
        data++;
    }    
    return 0;
}
void eputc(char c)
{
    while( (USART2->ISR & (1 << 6))==0);     // wait for ongoing transmission to finish
    USART2->TDR=c;                          //load character in Transmission data register to send over USART2 - sends to serial monitor in this case 
} 



void sendMessage()
{
    //function used to send accelerometer data to recieiving board via usart1
  
    char msg[48];
    snprintf(msg, sizeof(msg), "X=%d,Y=%d,Z=%d", X_g, Y_g, Z_g);  // live accel values
  //  printf("Sending: [%s]\r\n", msg);

    // Send opening bracket
    while (!(USART1->ISR & (1 << 6)));  // Wait until TXE = 1
    USART1->TDR = '[';
    //printf("Sent: [\r\n");
    delay(1000);

    // Send message contents
    for (int i = 0; msg[i] != '\0'; i++) {
        while (!(USART1->ISR & (1 << 6)));  // Wait until TXE = 1
        USART1->TDR = msg[i];
        delay(1000);
        //  printf("Sent: %c\r\n", msg[i]);     // Debug: print each character
  
    }

    // Send closing bracket
    while (!(USART1->ISR & (1 << 6)));
    USART1->TDR = ']';
  // printf("Sent: ]\r\n");
  delay(1000);

    while (!(USART1->ISR & (1 << 7)));      // Wait until TC = 1 (all sent)
    printf("message sent\r\n");
}


void clearMessage(char *buffer, int size)
{
   //function used to clear recieved message after it has been recieved and displayed.
    for (int i = 0; i < size; i++)
    {
        buffer[i] = '\0';
    }
}

void USART1_IRQHandler(void)
{
    //Interupt function handler - called when USART1 RX buffer is not empty
    if (USART1->ISR & (1 << 5))               // Check if RXNE (Receive Not Empty) flag is set
    {
    
        char c = USART1->RDR;               // Read the received character
         // Debug print
    //     printf("Char received: %c\r\n", c);
        if (c == '[')                      // Message recieved from board 2 will be enclosed in sqaure brackets
        {
            
            init_circ_buf(&rx_buf);         // Reset/clear the circular buffer
            message_started = 1;           // Set flag because message is being receiving
        }
        else if (c == ']')                //when final character of message is recieved
        {
            
            if (message_started)                                 
            {
                data_ready = 1;                            //data ready if flag set if message was being recieved - indicated by flag above
                message_started = 0;                      // Reset message recieving flag
                printf("Full message received!\r\n");     
            }
  
        }
        else
        {
            if (message_started)
            {
                put_circ_buf(&rx_buf, c);             // Store characters in circular buffer if recieving message flag is set and if end of message has not been reached
            }
        }
    }
}

void printMessage(int i,const char *message)
{
    //function used to display message on the LCD 
    shiftdisp(i,message);                                                  //shiftdisp called to handle line positioning of message
    fillRectangle(0,0,SCREEN_WIDTH, SCREEN_HEIGHT, 0x00);                 //lcd rectangle cleared by filling lcd with black rectangle
    for(int j=0;j<8;j++)
    {
        if (messagetype[j] == 0){                                                              //if message is a sent message
        printText(messagesdisp[j],30,(70-10*j), RGBToWord(255,255,255), RGBToWord(0,0,0));    //print it in white indented to the right
    }
        else if (messagetype[j] == 1)                                                        //if message is  a recieved message
        {
        printText(messagesdisp[j],0,(70-10*j), RGBToWord(0,255,0), RGBToWord(0,0,0));       //print it in green at with no identation 
        
    }
      
    }
}


void shiftdisp(int type,const char *message)
{
    //function used to handle how the messages are displayed on the LCD
    
    int messageLength = strlen(message);   
    int char_tolerance;
    if(type == 0)
    {char_tolerance = 18;}                        //the maximum amount of characters per line on the LCD are different depending on the type of message
    else                                         //message tpye 0 = a sent message which is indented on the LCD - the char tolerance is lower
    {char_tolerance = 23;}                      //message type 1 = a recieved message which is not indented - the char tolerance is higher
    
    
    //if the length of the message exceeds the maximum number of characters per line in the LCD, the message has to be split across multiple lines
    if(messageLength>char_tolerance)
    {
        
        int factor = messageLength/char_tolerance;          //find how many lines the message can fill up
        int k;
        if ((messageLength%char_tolerance)==0)            //if message length divides evenly into tolerance, message can fill the exact factor amount of lines up on the LCD
        {k = factor;}
        else{ k = factor+1;}                            //if there is a remainder present, remainer of characters will fill up an additional line

        int splitCount = 0;
        char splitMessage[k][char_tolerance];
        //step 1 - shift elements of message array display depending on number of lines the new message takes up
        for (int i = 7; i >= k ;i--) {
            strcpy(messagesdisp[i], messagesdisp[i-k]);       //each element in the message display array contains each line of characters that will be displayed
            messagetype[i] = messagetype[i-k];                 //each element in the message type array contain whether each line of characters the will be display (sent or recieved)
        }
        //step 2 - Clear the spaces where the new message will be written 
        for (int i = 0; i < k; i++) {
            memset(messagesdisp[i], ' ', char_tolerance);      //Clear elements i n array and fill with spaces to ensure no characters are left over from previous message
            messagetype[i] = type;
            messagesdisp[i][char_tolerance] = '\0';            //add null terminator to show where string ends
        }
        //step 3 - fill splitMessage array with each element being each section that will correspond to seperate lines in the LCD
        for (int q = 0; splitCount<k; q+=char_tolerance) {                           //loops in steps of message lines
            
            for (int j = 0; j < char_tolerance;j++) {
                splitMessage[splitCount][j] = message[j+q];                              //characters for each split of the message is copied into seperate elements of the splitMessage array
               
            }
           
           splitMessage[splitCount][char_tolerance] = '\0';                              // Null-terminate each chunk of characters
          
           strcpy(messagesdisp[k-(splitCount+1)], splitMessage[splitCount]);
           messagesdisp[k-splitCount+1][char_tolerance] = '\0';
           splitCount++;                                                                
        }
 
      
      
}
else   
{
    
        
    //the case when the message is enough characters to fit on one line of the LCD
        for (int i = 7; i >= 1; i--) {
            strcpy(messagesdisp[i], messagesdisp[i-1]);              //shift message display elements down by 1
            messagetype[i] = messagetype[i-1];                      //shift message type elements down by 1
        } 
    

   // clear space in first element of display array for new message to be displayed
        memset(messagesdisp[0], ' ', char_tolerance);             
        messagesdisp[0][char_tolerance] = '\0';  

    //copy new message into first element of message display and type arrays   
       strcpy(messagesdisp[0], message);
       messagetype[0] = type;
  
}
}    

//button pressed button check for manually send data button
int buttonpressed(int buttonpin)
{
    if ((GPIOB->IDR & (1 << buttonpin)) == 0) // check PB0 instead of PB4
    {
        delay(50000); // simple debounce
        return 1;
    }
    else
    {
        return 0;
    }
}


//function used to retrieve accelerometer values - void function used as x,y,z are global functions
void measureAccel() {
         //X VALUE
         printf("Reading X axis...\n");
         GPIOB->ODR |= (1 << 3);	                   // set port bit for logic analyser debug
         I2CStart(0x69,WRITE,1);                      // Write the address of the 
         I2CWrite(0x12);                           	// register we want to talk to
         I2CReStart(0x69,READ,2);                  // Switch to read mode and request 2 bytes
         response=I2CRead();		              // read low byte
         x_accel=response;  	    
         response=I2CRead();		            // read high byte
         x_accel=x_accel+(response << 8);      // combine bytes
 
         //Y VALUE
         printf("Reading Y axis...\n");
         I2CStart(0x69,WRITE,1);                // Write the address of the 
         I2CWrite(0x14);      	               // register we want to talk to
         I2CReStart(0x69,READ,2);             // Switch to read mode and request 2 bytes
         response=I2CRead();		         // read low byte
         y_accel=response;  	
         response=I2CRead();		       // read high byte
         y_accel=y_accel+(response << 8); // combine bytes
 
         //Z VALUE
         printf("Reading Z axis...\n");
         I2CStart(0x69,WRITE,1);              // Write the address of the 
         I2CWrite(0x16);      	             // register we want to talk to
         I2CReStart(0x69,READ,2);           // Switch to read mode and request 2 bytes
         response=I2CRead();		       // read low byte
         z_accel=response;  	
         response=I2CRead();		      // read high byte
         z_accel=z_accel+(response << 8); // combine bytes
 
        
         
 
 
         // end the tranmission and convert values, then print all 3
         I2CStop();	                               // end I2C transaction
         GPIOB->ODR &= ~(1 << 3);                 // clear port bit for logic analyser debug
         X_g = x_accel;	                         // promote to 32 bits and preserve sign
         X_g=(X_g*981)/16384;                   // assuming +1g ->16384 (+/-2g range)
         Y_g = y_accel;	                       // promote to 32 bits and preserve sign
         Y_g=(Y_g*981)/16384;                 // assuming +1g ->16384 (+/-2g range)
         Z_g = z_accel;	                     // promote to 32 bits and preserve sign
         Z_g=(Z_g*981)/16384;               // assuming +1g ->16384 (+/-2g range)
         
    
     enable_Transmit(4,5);   
     delay_ms(10000);
     delay_ms(100000);  // Debounce delay
}
void EXTI1_IRQHandler(void) //interrupt function for button
{
    if (EXTI->PR1 & (1 << 1))  // Check if EXTI line 1 triggered
    {
        EXTI->PR1 = (1 << 1);  // Clear the interrupt pending flag for EXTI1

        ack_recieved = 1;
        if (pongMode == 1)
        {
            pongMode = 0;                   //disable pong mode
            enable_Transmit(4,5);          //set transceiver to transmit mode
            sendPongMessage(pongMode);    //Notify recieving board to resume sending response ACKs
        }
        else if(pongMode == 0)
        {
        pongMode = 1;          // Enable Pong mode
        }
        printf("Button Interrupt...\r\n");
    }
    
}

//function used to send message to other board that pong mode has been triggered or exited
void sendPongMessage(int pongmode)
{
    const char *msg; 
    
    //determine the message sent to the other board based on the pongmode value
    // 1 = in pong mode which means that this board needs to tell the other board to stop sending acks and keep receiving data 
    // 2 = exiting pong mode which measns that this board needs to tell the other board to start sending acks again
    if (pongmode == 1){
    msg = "  [PONG]";
    }
 
    else
    {
    msg = "  [EXIT]";
    }

    for (int i = 0; msg[i] != '\0'; i++) {
        while (!(USART1->ISR & (1 << 6)));  // Wait until TXE = 1
        USART1->TDR = msg[i];
       printf("Sent: %c\n", msg[i]);       // Debug: print each character
        delay(100000);
    }
    while (!(USART1->ISR & (1 << 7)));      // Wait until TC = 1 (all sent)
    
}
