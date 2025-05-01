# **EmotiComm: A Bidirectional Accelerometer Messaging System**
## **Introduction**

This project demonstrates communication between two STM32 Nucleo boards using UART over RS-485 transcievers. 
The goal is to transmit live acclerometer data and status messages, visualize them on an LCD screen, and interact using onboard buttons to toggle between modes like Pong and Smiley Face display. 
The previous project "Electric Tin-can Telephone" was used as foundation for this project. 

### **Key Features:**
- **RS-485 UART Communication** : Full-duplex messaging using SN75176A transceivers.
- **Accelerometer Input**: Reads real-time X, Y, Z data from a BMI160 sensor via I²C.
- **Dynamic Display** : LCD shows live data and graphical feedback.
- **Pong Mode** : Rapid data sending with no ACK wait, triggered by a button press.
- **Display Mode Toggle:** :Button-triggered switch between smiley orientation and other display modes.
- **Reliable Data Handling** : Uses circular buffers and interrupts for message integrity.

## **System Architecture**

The system consists of two Nucleo boards:
1. **Board 1** : Sends live accelerometer data and listens for ACKs or Pong requests.
2. **Board 2**: Receives, parses, and visualizes incoming data. Controls display modes.
Communication uses RS-485 transceivers with DE/RE control for direction switching.
Display and sensor modules are connected via SPI and I²C respectively.

## **Data Flow & Message Handling**
### **Board 1: Sender**
- Reads data from BMI160 accelerometer (I²C).
- Sends [X=...,Y=...,Z=...] formatted strings over UART (RS-485).
- Waits for an ACK before resending, unless in Pong Mode.
- Button 1: Triggers message resend (if ACK missing).
- Button 2: Activates Pong Mode — sends data rapidly with no ACK wait.

### **Board 2: Reciever**
- Receives and parses formatted messages.
- Displays data or graphical output (e.g., smiley face orientation).
- Sends [Ack] or other responses back to Board 1.
- Button 3: Toggles display modes (e.g Acceleration value view to graphical smiley)

## **Hardware & Pin Configuration**
The system has the following interfaces:
![image](https://github.com/user-attachments/assets/15ee6ecb-daf3-4b42-a9dc-b1a01ef54b0c)
*Figure 1: Excel Sheet showing the pin mapping for entire system*

## Circuit Diagram  

The Kicad Schematic below shows the design of the entire system.
![image](https://github.com/user-attachments/assets/375ef828-f049-47ae-8156-80ab4a87894c)
*Figure 2: Kicad Schematic showing entire system layout*

### **Bidirectional Transcievers (RS-485)**
![image](https://github.com/user-attachments/assets/a85c7917-c7f7-4461-9a80-03a5082d9e72)
*Figure 3: Bidirectional Transcievers (RS-485)*

- The RS-485 is more reliable for longer distance and noisy environments than regular USART. 
It uses a differential signaling (A and B pins) which is more robust than USART.

- The transceivers are powered at 5V, while the STM32L432KC runs at 3.3V. 
We were initially unsure if a voltage divider was needed to protect the RX pin on the MCU.
However, the STM32L432KC datasheet confirms the relevant pins are 5V tolerant, meaning it’s safe to connect the 5V signal directly.
- To make sure the signal doesn't relect back along the wire, a 120Ω resistor is placed across pins A and B. 
This improves the stability over long wires, without these resistors, the signal would bounce back and cause corrupted data.

![image](https://github.com/user-attachments/assets/9a60e433-b01a-48f1-8175-53d854ab69a9)

*Figure 4: STM32 Pinout Table*

![image](https://github.com/user-attachments/assets/6f076c12-eb8e-4f05-986d-a5abb84a0ad3)

*Figure 5: Confirming the 5V tolerant p*

The pinmodes were set:
```c
pinMode(GPIOB, 4, 1);  // RE pin as output
pinMode(GPIOB, 5, 1);  // DE pin as output
```
The functions for direction control were created in their own seperate file:
```c
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
```
### **ST7735S LCD Displays**
![image](https://github.com/user-attachments/assets/5c7ff930-c694-4a42-84ff-d7a7650cba36)

*Figure 6: Schematic for the LCD displays*

**Recieving Board 2:**
Board 2 uses the ST7735S LCD display to show different visual outputs depending on the current display mode.
Modes:
- Text Mode (default mode):
  - Shows messages like "[X=...,Y=...,Z=...]" received from Board 1.
  - Uses printText() or printTextX2() to display characters on-screen.
- Smiley Mode
  - Displays a yellow smiley face that rotates based on the board's orientation.
  - Called using drawSmiley(position) with different positions for UP, LEFT, RIGHT, and DOWN.
  - Orientation is determined from the accelerometer data sent by Board 1.
- Pong Mode
  - LCD displays a simple pong game.
  - The paddle moves based on live X/Y acceleration.
  - The red ball bounces around, and the score increases with each bounce off the top or paddle.
  - Controlled by the moveGame(x, y) function.
  - ACK is disabled during Pong Mode to increase speed — this lets Board 1 send data continuously without waiting for a reply.
 
### **BM160 Accelerometer**
![image](https://github.com/user-attachments/assets/44bd3ddf-a14e-4dea-80ad-a17c4087eee0)

*Figure 7: Schematic for the Accelerometer*

The BMI160 accelerometer was used to detect motion and orientation by reading the X, Y, and Z-axis acceleration values over I²C communication.
In order to write to the correct registers for the acceleration values, the datasheet was consulted.

![image](https://github.com/user-attachments/assets/691689c8-f282-4e49-b3d8-81a13127ab32)

*Figure 8: Page 52 of the BM160 Datasheet showing the relavant registers for accel values*

The accelerometer gives you a 16-bit value, but it can only send 8 bits at a time over I²C.
So it splits the number into two parts:
- LSB (Least Significant Byte) – the lower part of the number
- MSB (Most Significant Byte) – the upper part

The LSB is sent first, because that’s how most microcontrollers expect data, from smallest to largest.
Later, both parts are combined in code to get the full 16-bit value.

**Mode Switching:**
A single push button cycles between 3 modes:
 - 0 (Default text displayed)
 - 1 (Smiley face tilting)
 - 2 (Pong game)
The code uses previousButton and currentButton to detect a new press (not a held button).
On each press, it changes the mode, clears the screen and loads the next mode.

In Mode 1, the receiver uses the X and Y values sent from the sender board to figure out how the sender is tilted.
This is done using a threshold value of ±500.

Why 500?
- The BMI160 accelerometer outputs values from about -16,384 to +16,384 in ±2g mode.
- We tested different values and found that ±500 was enough to detect a clear tilt without reacting to small shakes or noise.
- It strikes a balance between sensitivity and stability.

How we used it:
```c
if (x_val > threshold) {
                   next_position = 1;                  //sender board tilted to the right       
               } else if (x_val < -threshold) {
                   next_position = 4;                 //sender board tilted to the left
               } else if (y_val > threshold) {
                   next_position = 3;                 //sender board tilted to backwards
               } else if (y_val < -threshold) {
                   next_position = 2;                 //sender board tilted to the forwards
               } else {
                   next_position = 1;                 //default positioning no major tilt
               }
```
These directions were then passed to the `drawSmiley(position)` function to update the orientation of the face.

The `moveGame(accel_x, accel_y)` function creates a live pong game using the X and Y tilt data send from the accelerometer.
The function does the follwing:
- Move the paddle using acceleration values
- Animate the ball across the screen
- Detect collisions and update score
- Erase and redraw game elements each frame

The ball moves every 7 times the function is run ( it is running continuously when this mode is active) in order to slow it down. 
```c
if (frame_counter >= 7)
```
The paddles moves based on how the sender board is tilted (accelerometer).
```c
paddle_x = (accel_x + 500) * SCREEN_WIDTH / 1000;
paddle_y = (accel_y + 500) * SCREEN_HEIGHT / 1000;
```
- This takes the X and Y acceleration from the sender board
- Maps it to a value that fits on the LCD screen
- So when the board tilts right, the paddle moves right, etc.
It also makes sure the paddle stays on the screen by checking the edges.

The code then checks if the ball is touching the paddle:
```c
if (ball overlaps paddle)
```
If true, the ball bounces back upwards and is moved slightly above the paddle so it doesnt stick. 
The score increases each time the ball hits the top of the screen. If it misses the paddle and hits the bottom of the screen, the score will reset to 0.
1. The previous score are is cleared:
```c
fillRectangle(SCREEN_WIDTH - 40, 0, 40, 10, black);
```
2. The updated score is printed using:
```c
printText(scoreStr, SCREEN_WIDTH - 30, 0, white, black);
```
### **STM32 Microcontrollers**
![image](https://github.com/user-attachments/assets/42d0791a-0d11-479b-9630-b062ef380005)

*Figure 9: Schematic for STM32 Nucleo Boards*



