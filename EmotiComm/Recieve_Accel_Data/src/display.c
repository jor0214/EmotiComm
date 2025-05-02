#include <display.h>
#include <stdint.h>
#include <stm32l432xx.h>
#include "eeng1030_lib.h"
#include "font5x7.h"
#include "spi.h"
#include <stdbool.h>
#define SCREEN_HEIGHT 80
#define SCREEN_WIDTH 160



/*	I/O List
		PA7  : SPI1 MOSI : Alternative function 5
        PA6  : Reset : GPIO Output
        PA5  : D/C : GPIO Output
		PA4  : CS : GPIO Output
		PA1  : SPI1 SCLK : Alternative function 5    
*/
static void CSLow(void);
static void CSHigh(void);
static void DCLow(void);
static void DCHigh(void);
static void ResetHigh(void);
static void ResetLow(void);
static void command(uint8_t cmd);
static void data(uint8_t data);
void clear(void);
static uint32_t mystrlen(const char *s);
static void drawLineLowSlope(uint16_t x0, uint16_t y0, uint16_t x1,uint16_t y1, uint16_t Colour);
static void drawLineHighSlope(uint16_t x0, uint16_t y0, uint16_t x1,uint16_t y1, uint16_t Colour);
static int iabs(int x);
static void openAperture(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void putPixel(uint16_t x, uint16_t y, uint16_t colour);
void putImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *Image, int hOrientation, int vOrientation);
void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t Colour);
void fillRectangle(uint16_t x,uint16_t y,uint16_t width, uint16_t height, uint16_t colour);

void drawPixel(int x, int y, uint16_t color);
void drawArc(int xc, int yc, int rx, int ry, int start_angle, int end_angle, uint16_t color);

// Global Ball variables
int score = 0;
int ball_x = 80;
int ball_y = 40;
int ball_dx = 1;
int ball_dy = 1;


void init_display()
{
    pinMode(GPIOA,3,1);
    pinMode(GPIOA,8,1);
    pinMode(GPIOA,4,1);
    initSPI(SPI1);
    // Lots of CS toggling here seems to have made the boot up more reliable
	ResetHigh();	
	delay_ms(10);
	ResetLow();
	delay_ms(200);
	ResetHigh();
	CSHigh();
	delay_ms(200);
	CSLow();
	delay_ms(20);
	command(0x01); // software reset
	delay_ms(100);
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0x11); // exit sleep
	delay_ms(120);
	// set frame rate
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0xb1);
	data(0x05);
	data(0x3c);
	data(0x3c);
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0xb2);
	data(0x05);
	data(0x3c);
	data(0x3c);
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0xb3);
	data(0x05);
	data(0x3c);
	data(0x3c);	
	data(0x05);
	data(0x3c);
	data(0x3c);
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0xb4); // dot invert
	data(0x03);
	CSHigh();
	delay_ms(1);			
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0x36);// Set pixel and RGB order
	data(0x60); 	
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);		
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0x3a);// Set colour mode        
	data(0x5); 	// 16 bits per pixel
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
	command(0x29);    // display on
	delay_ms(100);
	CSHigh();
	delay_ms(1);
	CSLow();
	delay_ms(1);
    command(0x21);
    delay_ms(1);
    command(0x2c);   // put display in to write mode
	fillRectangle(0,0,SCREEN_WIDTH, SCREEN_HEIGHT, 0x00);  // black out the screen

}
static void CSHigh(void)
{
    GPIOA->ODR |= (1 << 4);
}
static void CSLow(void)
{
    GPIOA->ODR &= ~(1 << 4);
}
static void DCHigh(void)
{
    GPIOA->ODR |= (1 << 8);
}
static void DCLow(void)
{
    GPIOA->ODR &= ~(1 << 8);
}
static void ResetHigh(void)
{
    GPIOA->ODR |= (1 << 3);
}
static void ResetLow(void)
{
    GPIOA->ODR &= ~(1 << 3);
}

static void command(uint8_t cmd)
{
    DCLow();
	transferSPI8(SPI1,cmd);
}
static void data(uint8_t data)
{
    DCHigh();
	transferSPI8(SPI1,data);
}

void openAperture(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    // open up an area for drawing on the display  
    x1 = x1 +1;
    x2 = x2+ 1;  
    y1 = y1+26;
    y2 = y2+26;
	command(0x2A); // Set X limits    	
    data(x1>>8);
    data(x1&0xff);        
    data(x2>>8);
    data(x2&0xff);
    
    command(0x2B);// Set Y limits
    data(y1>>8);
    data(y1&0xff);        
    data(y2>>8);
    data(y2&0xff);    
        
    command(0x2c); // put display in to data write mode
	
}
void fillRectangle(uint16_t x,uint16_t y,uint16_t width, uint16_t height, uint16_t colour)
{
	uint32_t pixelcount = height * width;
	openAperture(x, y, x + width - 1, y + height - 1);
	DCHigh();
	while(pixelcount--) 
	{
		transferSPI16(SPI1,colour);
	}	
}
void putPixel(uint16_t x, uint16_t y, uint16_t colour)
{
	openAperture(x, y, x + 1, y + 1);	
	DCHigh();
	transferSPI16(SPI1,colour);
}
void delay_ms(uint32_t ms)
{
    delay(ms * 80000);  // assumes 80MHz system clock
}
void putImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *Image, int hOrientation, int vOrientation)
{
    uint16_t Colour;
	  uint32_t offset = 0;
    openAperture(x, y, x + width - 1, y + height - 1);
    DCHigh();
	  if (hOrientation == 0)
		{
			if (vOrientation == 0)
			{
				for (y = 0; y < height; y++)
				{
						offset=y*width;
						for (x = 0; x < width; x++)
						{
								Colour = Image[offset+x];
								transferSPI16(SPI1,Colour);
						}
				}
			}
			else
			{
				for (y = 0; y < height; y++)
				{
						offset=(height-(y+1))*width;
						for (x = 0; x < width; x++)
						{
								Colour = Image[offset+x];
								transferSPI16(SPI1,Colour);
						}
				}
			}
		}
		else
		{
			if (vOrientation == 0)
			{
				for (y = 0; y < height; y++)
				{
						offset=y*width;
						for (x = 0; x < width; x++)
						{
								Colour = Image[offset+(width-x-1)];
								transferSPI16(SPI1,Colour);
						}
				}
			}
			else
			{
				for (y = 0; y < height; y++)
				{
						offset=(height-(y+1))*width;
						for (x = 0; x < width; x++)
						{
								Colour = Image[offset+(width-x-1)];
								transferSPI16(SPI1,Colour);
						}
				}
			}
		}
}
void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t Colour)
{
	// Reference : https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm    
    if ( iabs(y1 - y0) < iabs(x1 - x0) )
    {
        if (x0 > x1)
        {
            drawLineLowSlope(x1, y1, x0, y0, Colour);
        }
        else
        {
            drawLineLowSlope(x0, y0, x1, y1, Colour);
        }
    }
    else
    {
        if (y0 > y1) 
        {
            drawLineHighSlope(x1, y1, x0, y0, Colour);
        }
        else
        {
            drawLineHighSlope(x0, y0, x1, y1, Colour);
        }
        
    }    
}
int iabs(int x) // simple integer version of abs for use by graphics functions        
{
	if (x < 0)
		x = -x;
	return x;
}
void drawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t Colour)
{
	drawLine(x,y,x+w,y,Colour);
  drawLine(x,y,x,y+h,Colour);
  drawLine(x+w,y,x+w,y+h,Colour);
  drawLine(x,y+h,x+w,y+h,Colour);
}
void drawCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t Colour)
{
// Reference : https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
    uint16_t x = radius-1;
    uint16_t y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);
    if (radius > x0)
        return; // don't draw even parially off-screen circles
    if (radius > y0)
        return; // don't draw even parially off-screen circles
        
    if ((x0+radius) > SCREEN_WIDTH)
        return; // don't draw even parially off-screen circles
    if ((y0+radius) > SCREEN_HEIGHT)
        return; // don't draw even parially off-screen circles    
    while (x >= y)
    {
        putPixel(x0 + x, y0 + y, Colour);
        putPixel(x0 + y, y0 + x, Colour);
        putPixel(x0 - y, y0 + x, Colour);
        putPixel(x0 - x, y0 + y, Colour);
        putPixel(x0 - x, y0 - y, Colour);
        putPixel(x0 - y, y0 - x, Colour);
        putPixel(x0 + y, y0 - x, Colour);
        putPixel(x0 + x, y0 - y, Colour);

        if (err <= 0)
        {
            y++;
            err += dy;
            dy += 2;
        }
        
        if (err > 0)
        {
            x--;
            dx += 2;
            err += dx - (radius << 1);
        }
    }
}
void fillCircle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t Colour)
{
	// Reference : https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
	// Similar to drawCircle but fills the circle with lines instead
    uint16_t x = radius-1;
    uint16_t y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    if (radius > x0)
        return; // don't draw even parially off-screen circles
    if (radius > y0)
        return; // don't draw even parially off-screen circles
        
    if ((x0+radius) > SCREEN_WIDTH)
        return; // don't draw even parially off-screen circles
    if ((y0+radius) > SCREEN_HEIGHT)
        return; // don't draw even parially off-screen circles        
    while (x >= y)
    {
        drawLine(x0 - x, y0 + y,x0 + x, y0 + y, Colour);        
        drawLine(x0 - y, y0 + x,x0 + y, y0 + x, Colour);        
        drawLine(x0 - x, y0 - y,x0 + x, y0 - y, Colour);        
        drawLine(x0 - y, y0 - x,x0 + y, y0 - x, Colour);        

        if (err <= 0)
        {
            y++;
            err += dy;
            dy += 2;
        }
        
        if (err > 0)
        {
            x--;
            dx += 2;
            err += dx - (radius << 1);
        }
    }
}
void printText(const char *Text,uint16_t x, uint16_t y, uint16_t ForeColour, uint16_t BackColour)
{
	// This function draws each character individually.  It uses an array called TextBox as a temporary storage
    // location to hold the dots for the character in question.  It constructs the image of the character and then
    // calls on putImage to place it on the screen
    uint8_t Index = 0;
    uint8_t Row, Col;
    const uint8_t *CharacterCode = 0;    
    uint16_t TextBox[FONT_WIDTH * FONT_HEIGHT];
	uint16_t len;
	len=(uint16_t) mystrlen(Text);
    for (Index = 0; Index < len; Index++)
    {
        CharacterCode = &Font5x7[FONT_WIDTH * (Text[Index] - 32)];
        Col = 0;
        while (Col < FONT_WIDTH)
        {
            Row = 0;
            while (Row < FONT_HEIGHT)
            {
                if (CharacterCode[Col] & (1 << Row))
                {
                    TextBox[(Row * FONT_WIDTH) + Col] = ForeColour;
                }
                else
                {
                    TextBox[(Row * FONT_WIDTH) + Col] = BackColour;
                }
                Row++;
            }
            Col++;
        }
        putImage(x, y, FONT_WIDTH, FONT_HEIGHT, (uint16_t *)TextBox,0,0);
        x = x + FONT_WIDTH + 2;
    }
}
void printTextX2(const char *Text, uint16_t x, uint16_t y, uint16_t ForeColour, uint16_t BackColour)
{
	#define Scale 2
	// This function draws each character individually scaled up by a factor of 2.  It uses an array called TextBox as a temporary storage
    // location to hold the dots for the character in question.  It constructs the image of the character and then
    // calls on putImage to place it on the screen
    uint8_t Index = 0;
    uint8_t Row, Col;
    const uint8_t *CharacterCode = 0;    
	uint16_t len;
	len=(uint16_t)mystrlen(Text);
    uint16_t TextBox[FONT_WIDTH * FONT_HEIGHT*Scale * Scale];

    for (Index = 0; Index < len; Index++)
    {
        CharacterCode = &Font5x7[FONT_WIDTH * (Text[Index] - 32)];
        Col = 0;
        while (Col < FONT_WIDTH)
        {
            Row = 0;
            while (Row < FONT_HEIGHT)
            {
                if (CharacterCode[Col] & (1 << Row))
                {
                    TextBox[((Row*Scale) * FONT_WIDTH*Scale) + (Col*Scale)] = ForeColour;
					TextBox[((Row*Scale) * FONT_WIDTH*Scale) + (Col*Scale)+1] = ForeColour;
					TextBox[(((Row*Scale)+1) * FONT_WIDTH*Scale) + (Col*Scale)] = ForeColour;
					TextBox[(((Row*Scale)+1) * FONT_WIDTH*Scale) + (Col*Scale)+1] = ForeColour;
                }
                else
                {
                    TextBox[((Row*Scale) * FONT_WIDTH*Scale) + (Col*Scale)] = BackColour;
					TextBox[((Row*Scale) * FONT_WIDTH*Scale) + (Col*Scale)+1] = BackColour;
					TextBox[(((Row*Scale)+1) * FONT_WIDTH*Scale) + (Col*Scale)] = BackColour;
					TextBox[(((Row*Scale)+1) * FONT_WIDTH*Scale) + (Col*Scale)+1] = BackColour;
                }
                Row++;
            }
            Col++;
        }
        putImage(x, y, FONT_WIDTH*Scale, FONT_HEIGHT*Scale, (uint16_t *)TextBox,0,0);
        x = x + FONT_WIDTH*Scale + 2;
    }
}
void printNumber(uint16_t Number, uint16_t x, uint16_t y, uint16_t ForeColour, uint16_t BackColour)
{
	// This function converts the supplied number into a character string and then calls on puText to
    // write it to the display
    char Buffer[6]; // Maximum value = 65535
    Buffer[5]=0;
    Buffer[4] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[3] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[2] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[1] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[0] = Number % 10 + '0';
    printText(Buffer, x, y, ForeColour, BackColour);
}
void printNumberX2(uint16_t Number, uint16_t x, uint16_t y, uint16_t ForeColour, uint16_t BackColour)
{
     // This function converts the supplied number into a character string and then calls on puText to
    // write it to the display
    char Buffer[6]; // Maximum value = 65535
    Buffer[5]=0;
    Buffer[4] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[3] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[2] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[1] = Number % 10 + '0';
    Number = Number / 10;
    Buffer[0] = Number % 10 + '0';
    printTextX2(Buffer, x, y, ForeColour, BackColour);	
}
uint16_t swap_bytes(uint16_t val)
{
    uint16_t b1,b2;
    b1 = val & 0xff;
    b2 = val >> 8;
    return (b1 << 8)+b2;
}
uint16_t RGBToWord(uint16_t R, uint16_t G, uint16_t B)
{
	uint16_t rvalue = 0;
    rvalue += G >> 5;
    rvalue += (G & (0x1c)) << 11;
    rvalue += (R >> 3) << 8;
    rvalue += (B >> 3) << 3;
    return rvalue;
}

void drawLineLowSlope(uint16_t x0, uint16_t y0, uint16_t x1,uint16_t y1, uint16_t Colour)
{
   // Reference : https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm    
  int dx = x1 - x0;
  int dy = y1 - y0;
  int yi = 1;
  if (dy < 0)
  {
    yi = -1;
    dy = -dy;
  }
  int D = 2*dy - dx;
  
  int y = y0;

  for (int x=x0; x <= x1;x++)
  {
    putPixel((uint16_t)x,(uint16_t)y,Colour);    
    if (D > 0)
    {
       y = y + yi;
       D = D - 2*dx;
    }
    D = D + 2*dy;
    
  }
}
void drawLineHighSlope(uint16_t x0, uint16_t y0, uint16_t x1,uint16_t y1, uint16_t Colour)
{
  // Reference : https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
  int dx = x1 - x0;
  int dy = y1 - y0;
  int xi = 1;
  if (dx < 0)
  {
    xi = -1;
    dx = -dx;
  }  
  int D = 2*dx - dy;
  int x = x0;

  for (int y=y0; y <= y1; y++)
  {
    putPixel((uint16_t)x,(uint16_t)y,Colour);
    if (D > 0)
    {
       x = x + xi;
       D = D - 2*dy;
    }
    D = D + 2*dx;
  }
}
void clear()
{
	fillRectangle(0,0,SCREEN_WIDTH, SCREEN_HEIGHT, 0x0000);  // black out the screen
}
uint32_t mystrlen(const char *s)
{
	uint32_t len=0;
	while(*s)
	{
		s++;
		len++;
	}
	return len;
}



#define PADDLE_WIDTH 20
#define PADDLE_HEIGHT 4
#define BALL_SIZE 8

void moveGame(int accel_y, int accel_x) // <-- note swapped accel
{
    static int old_paddle_x = 0;
    static int old_paddle_y = 0;
    static int old_ball_x = 0;
    static int old_ball_y = 0;
    static int frame_counter = 0;

// inside moveGame() at the start
frame_counter++;
if (frame_counter >= 7) // move ball every 5 frames
{
    frame_counter = 0;
    ball_x += ball_dx;
    ball_y += ball_dy;
        // 4. Bounce off screen edges
        if (ball_x <= 0 || ball_x >= SCREEN_WIDTH - BALL_SIZE) {
            ball_dx = -ball_dx;
        }
        if (ball_y <= 0) {
            ball_dy = -ball_dy;
            score = score+1;
        }
        if (ball_y >= SCREEN_HEIGHT - BALL_SIZE) {
            ball_dy = -ball_dy; // or reset game
            score = 0;
        }
        
        fillRectangle(SCREEN_WIDTH - 40, 0, 40, 10, RGBToWord(0, 0, 0)); // clear old score
        char scoreStr[10];
        sprintf(scoreStr, "%d", score);
        printText(scoreStr,SCREEN_WIDTH - 30, 0, RGBToWord(255, 255, 255), RGBToWord(0, 0, 0));
}

    // 1. Move paddle
    int paddle_x = (accel_x + 500) * SCREEN_WIDTH / 1000;
    int paddle_y = (accel_y + 500) * SCREEN_HEIGHT / 1000;

    // Bound paddle inside screen
    if (paddle_x < 0) paddle_x = 0;
    if (paddle_x >= SCREEN_WIDTH - PADDLE_WIDTH) paddle_x = SCREEN_WIDTH - PADDLE_WIDTH;
    if (paddle_y < 0) paddle_y = 0;
    if (paddle_y >= SCREEN_HEIGHT - PADDLE_HEIGHT) paddle_y = SCREEN_HEIGHT - PADDLE_HEIGHT;

    // 2. Erase old paddle and ball (draw over with black)
    fillRectangle(old_paddle_x, old_paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, RGBToWord(0, 0, 0));
    fillRectangle(old_ball_x, old_ball_y, BALL_SIZE, BALL_SIZE, RGBToWord(0, 0, 0));




    // 5. Bounce off paddle
    bool ball_hit_paddle = false;

    // Check if ball overlaps paddle (including sides)
    if (ball_y + BALL_SIZE >= paddle_y && ball_y <= paddle_y + PADDLE_HEIGHT &&
        ball_x + BALL_SIZE >= paddle_x && ball_x <= paddle_x + PADDLE_WIDTH)
    {
        ball_hit_paddle = true;
    }

    if (ball_hit_paddle)
    {
        ball_dy = -ball_dy;
        ball_y = paddle_y - BALL_SIZE; // push ball just above the paddle
    }

    // 6. Draw paddle and ball
    fillRectangle(paddle_x, paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, RGBToWord(0, 255, 0)); // Green paddle
    fillRectangle(ball_x, ball_y, BALL_SIZE, BALL_SIZE, RGBToWord(255, 0, 0)); // Red ball

    // 7. Save old positions for next frame
    old_paddle_x = paddle_x;
    old_paddle_y = paddle_y;
    old_ball_x = ball_x;
    old_ball_y = ball_y;
}



void drawPixel(int x, int y, uint16_t color) {
    fillRectangle(x, y, 1, 1, color);
}

void drawArc(int xc, int yc, int rx, int ry, int start_angle, int end_angle, uint16_t color) {
    float step = 1.0; // smaller = smoother curve
    for (float angle = start_angle; angle <= end_angle; angle += step) {
        float rad = angle * 3.14 / 180.0;
        int x = xc + (int)(rx * cos(rad));
        int y = yc + (int)(ry * sin(rad));
        drawPixel(x, y, color);
        drawPixel(x + 1, y, color);
        drawPixel(x, y + 1, color);
        drawPixel(x + 1, y + 1, color);
    }
}



