//include necessary header files
#include <eeng1030_lib.h>
#include <stdio.h>
#include  <errno.h>
#include  <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include "circular_buffer.h"
#include "display.h"
#include "biDirectional_Trans.h"
#include <string.h>


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
void delay(volatile uint32_t dly);
void initSerial(uint32_t baudrate);
void eputc(char c);
void readInputMessage();
void shiftdisp(int i,const char *message);
void sendMessage();
void clearMessage(char *buffer, int size);
void printMessage(int i,const char *message);
void shiftdisp(int type,const char *message);
void send_Ack();
void drawSmiley(int next_position);
int buttonpressed(void);

//variables declarations 
int count;
int currentLine = 0;
circular_buffer rx_buf;                             //Circular buffer structure defined in circular buffer.h
volatile uint32_t data_ready=0;                    //flag to indicate when meesage is finished being added to the buffer
volatile char input_buffer[INPUT_BUFFER_SIZE];    //stores user-typed message from serial port
volatile uint8_t input_index = 0;                //index variable to irerate through characters in input message
volatile uint8_t input_ready = 0;               //flag to indicate when input message is ready to be sent 
static volatile uint8_t message_started = 0;   //Flag to indicate when message has started being recieved 
char messagesdisp[8][24];            //stores ch line of messages to be printed to LCD
int messagetype[8];                 //stores whether message is recieved message or sent message to detemine how it is displayed on LCD
int current_position = 0;
int next_position = 0;



int main()
{

    char message_received[CIRC_BUF_SIZE];      //stores recieved message from circular buffer
    setup();                                  //call functions to setup and initialise the system
    init_display();
    init_circ_buf(&rx_buf);
    NVIC->ISER[1] |= (1 << (37-32));        //ensures that the USART1_IRQHandler() gets triggered when data is received via UART2
    enable_interrupts();
    fillRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBToWord(0, 0, 0)); // clear screen
    fillCircle(80, 40, 20, RGBToWord(255, 255, 0));
    int currentButton = 0;
    int previousButton = 0;
    int mode = 0;
    int threshold = 500;
    int x_val = 0;
    int y_val = 0;
    int z_val = 0;
    int pongMode = 0;
    printf("Receiver setup complete. Waiting for messages...\r\n");

    while(1)
    {
        currentButton = buttonpressed();
        if (previousButton == 0 && currentButton == 1)
        {
            printf("button pressed\r\n");
            if(mode == 0)
            {
                mode = 1;
                fillRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBToWord(0, 0, 0));
                drawSmiley(1);
            }
            else if(mode == 1)
            {
                mode = 3;
               fillRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBToWord(0, 0, 0));
            }
            else if(mode == 3)
            {
                mode = 0;
                fillRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBToWord(0, 0, 0));
            }
        }
        previousButton = currentButton;
        
           if(mode == 1)
           {
               if (x_val > threshold) {
                   next_position = 1;
               } else if (x_val < -threshold) {
                   next_position = 4;
               } else if (y_val > threshold) {
                   next_position = 3;
               } else if (y_val < -threshold) {
                   next_position = 2;
               } else {
                   next_position = 1;
               }
               if(!(current_position == next_position))
               {
                   drawSmiley(next_position);
                   current_position = next_position;
               }
           }
           else if (mode == 3)
           {
               moveGame(x_val, y_val);
           }
           


        if (data_ready)                                     //data ready flag controller by usart1 interupt handler
        {

            printf("Data ready flag set. Processing received message...\r\n");

            int i = 0;                                  
            char c;                                       
            while(get_circ_buf(&rx_buf, &c) == 0)         //get circ buffer fuction returns 0 when circular buffer is not empty    
            {
                message_received[i++] = c;              //next character of buffer is retrieved and stored in message array each time get_circ_buf function is called
                //printf("Char extracted from buffer: %c\r\n", c);
            }
           message_received[i+1] = '\0';  
           
           printf("Message received from USART1: %s\r\n", message_received);      //print to serial monitor (usart2) 
           
           if (strcmp(message_received, "PONG") == 0)
           {
            pongMode = 1;
           }
           else if(strcmp(message_received, "EXIT") == 0)
           {
            pongMode = 0;
           }
    
        else if (sscanf(message_received, "X=%d,Y=%d,Z=%d", &x_val, &y_val, &z_val) == 3) {
            
          //  printf("Parsed values - X: %d, Y: %d, Z: %d\r\n", x_val, y_val, z_val);
        } else {
            // If parsing fails, show raw message
            printf("Parsing failed. Displaying raw message.\r\n");
            printMessage(1, message_received);
        }
                                //call printMessage funtion to print to LCD
            
        if(mode == 0)
        {
            char xyz_buffer[32];

// Convert to mg and display
delay(10000000);
clear();  // Clear screen first

snprintf(xyz_buffer, sizeof(xyz_buffer), "X=%ld mg", x_val);
printMessage(1,xyz_buffer);


snprintf(xyz_buffer, sizeof(xyz_buffer), "Y=%ld mg", y_val);
printMessage(1,xyz_buffer);

snprintf(xyz_buffer, sizeof(xyz_buffer), "Z=%ld mg", z_val);
printMessage(1,xyz_buffer);
       }
            clearMessage(message_received, CIRC_BUF_SIZE);          //After message has been recieved, call function to clear message recieved function
            data_ready = 0; 
            delay(10000);
            if(pongMode == 0)
            {send_Ack(); }                                              //reset data flag to zero
        }

       

}
}
void delay(volatile uint32_t dly)
{
    while(dly--);
}
void setup()
{
    initClocks();    
    RCC->AHB2ENR |= (1 << 0) + (1 << 1);     // enable GPIOA and GPIOB
    //Changing led functionality pins to pins to control chip comms settings
    pinMode(GPIOB,4,1);  //Set pin pB4 to 1 - RE Transciever pin
    pinMode(GPIOB,5,1);  //Set pin pB5 to 1 -  DE Transciever pin
    pinMode(GPIOB,0,0);
    enablePullUp(GPIOB,0);
    initSerial(9600);
    enable_Recieve(4,5);
    
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
    //function is used to call function to send message to LCD. It is also adds sqaure brackets to the start and end of the message in the input
    //buffer and transmitts it through USART1.
    
    
       // GPIOB->ODR |= (1 << 5);   // DE = 1 → ENABLE TX
        //GPIOB->ODR &= ~(1 << 4);  // /RE = 0 → DISABLE RX (stay in TX mode)
    
        const char *msg = "  [Ack]";
    
        for (int i = 0; msg[i] != '\0'; i++) {
            while (!(USART1->ISR & (1 << 6)));  // Wait until TXE = 1
            USART1->TDR = msg[i];
      //     printf("Sent: %c\n", msg[i]);       // Debug: print each character
            delay(100000);
        }
        while (!(USART1->ISR & (1 << 7)));      // Wait until TC = 1 (all sent)
    
    
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
        printf("char rec %c  \r\n", c);
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


void send_Ack()
{
//enable tranmission when message is finished being received
    enable_Transmit(4,5);
    sendMessage();
    enable_Recieve(4,5);
    printf("Ack Sent!\r\n");
    
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

void drawSmiley(int next_position) {
    fillCircle(80, 40, 20, RGBToWord(255, 255, 0));
    if(next_position == 1)
    // Head
    {  // Yellow face

    // Eyes
    fillCircle(70, 35, 4, RGBToWord(0, 0, 0));
    fillCircle(90, 35, 4, RGBToWord(0, 0, 0));

    // Smile
    drawArc(80, 45, 12, 12, 20, 160, RGBToWord(0, 0, 0)); // semicircle smile
    }

    if(next_position == 2)
    {
    fillCircle(85, 30, 4, RGBToWord(0, 0, 0));
    fillCircle(85, 50, 4, RGBToWord(0, 0, 0));
    drawArc(75, 40, 12, 12, 110, 250, RGBToWord(0, 0, 0));
  }

  if(next_position == 3)
{
    fillCircle(75, 50, 4, RGBToWord(0, 0, 0));
    fillCircle(75, 30, 4, RGBToWord(0, 0, 0));
    drawArc(85, 40, 12, 12, 290, 430, RGBToWord(0, 0, 0));
}

if(next_position == 4) {
    fillCircle(70, 45, 4, RGBToWord(0, 0, 0));
    fillCircle(90, 45, 4, RGBToWord(0, 0, 0));

    drawArc(80, 35, 12, 12, 200, 340, RGBToWord(0, 0, 0));
}
}

int buttonpressed(void)
{
    if ((GPIOB->IDR & (1 << 0)) == 0) // check PB0 instead of PB4
    {
        delay(50000); // simple debounce
        return 1;
    }
    else
    {
        return 0;
    }
}



