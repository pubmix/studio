#include "Arduino.h"
#include "LCD.h"
#include <SPI.h>

#define  LCM_RESET  16
#define  LCM_CS     5
#ifndef LCM_SPI_HZ
#define LCM_SPI_HZ 8000000
#endif

// ------------------------------------------------------------ SPI Drive --------------------------------------------------------------------
#if Arduino_SPI
void ER5517Basic::SPIInit()
{
	pinMode(LCM_CS, OUTPUT);
	SPI.beginTransaction(SPISettings(LCM_SPI_HZ, MSBFIRST, SPI_MODE0));
	SPI.begin();
}
void lcmSetSpiHz(uint32_t hz)
{
	SPI.endTransaction();
	SPI.beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
}
void ER5517Basic::SPISetCs(int cs)
{
	if(cs)
		digitalWrite(LCM_CS,HIGH);
	else
	  digitalWrite(LCM_CS,LOW);
}
unsigned char ER5517Basic::SPIRwByte(unsigned char value)
{
	unsigned char rec;
	rec = SPI.transfer(value);
	return rec;
}
void ER5517Basic::SPI_CmdWrite(int cmd)
{
  ER5517.SPISetCs(0);    //SS_RESET;
  ER5517.SPIRwByte(0x00);
  ER5517.SPIRwByte(cmd);
  ER5517.SPISetCs(1);    //SS_SET;
}
void ER5517Basic::SPI_DataWrite(int data)
{
  ER5517.SPISetCs(0);    //SS_RESET;
  ER5517.SPIRwByte(0x80);
  ER5517.SPIRwByte(data);
  ER5517.SPISetCs(1);    //SS_SET;
}
int ER5517Basic::SPI_StatusRead(void)
{
  int temp = 0;
  ER5517.SPISetCs(0);    //SS_RESET;
  ER5517.SPIRwByte(0x40);
  temp = ER5517.SPIRwByte(0x00);
  ER5517.SPISetCs(1);    //SS_SET;
  return temp;
}

int ER5517Basic::SPI_DataRead(void)
{
  int temp = 0;
  ER5517.SPISetCs(0);    //SS_RESET;
  ER5517.SPIRwByte(0xc0);
  temp = ER5517.SPIRwByte(0x00);
  ER5517.SPISetCs(1);    //SS_SET;
  return temp;
}
#endif

//-----------------------------------------------------------------------------------------------------------------------------------

void ER5517Basic::Parallel_Init(void)
{
	#if Arduino_SPI
	ER5517.SPIInit();
	#endif
	
	#if Arduino_IIC
	ER5517.IICInit();
	#endif
}
void ER5517Basic::LCD_CmdWrite(unsigned char cmd)
{
	#if Arduino_SPI
	ER5517.SPI_CmdWrite(cmd);
	#endif
	
	#if Arduino_IIC
	ER5517.IIC_CmdWrite(cmd);
	#endif
}

void ER5517Basic::LCD_DataWrite(unsigned char data)
{
	#if Arduino_SPI
	ER5517.SPI_DataWrite(data);
	#endif
	
	#if Arduino_IIC
	ER5517.IIC_DataWrite(data);
	#endif
}

unsigned char ER5517Basic::LCD_StatusRead(void)
{
	unsigned char temp = 0;
	
	#if Arduino_SPI
	temp = ER5517.SPI_StatusRead();
	#endif
	
	#if Arduino_IIC
	temp = ER5517.IIC_StatusRead();
	#endif
	
	return temp;
}

unsigned int ER5517Basic::LCD_DataRead(void)
{
	unsigned int temp = 0;

	#if Arduino_SPI
	temp = ER5517.SPI_DataRead();
	#endif
	
	#if Arduino_IIC
	temp = ER5517.IIC_DataRead();
	#endif
	
	return temp;
}
void ER5517Basic::LCD_RegisterWrite(unsigned char Cmd,unsigned char Data)
{
	ER5517.LCD_CmdWrite(Cmd);
	ER5517.LCD_DataWrite(Data);
}  
//---------------------//

void ER5517Basic::Check_SDRAM_Ready(void)
{
/*  0: SDRAM is not ready for access
  1: SDRAM is ready for access    */  
  unsigned char temp;
  do
  {
    temp=ER5517.LCD_StatusRead();
  }
  while( (temp&0x04) == 0x00 );
}
void ER5517Basic::TFT_24bit(void)
{
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x01);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb4;
  temp &= cClrb3;
  ER5517.LCD_DataWrite(temp);  
}

void ER5517Basic::Host_Bus_16bit(void)
{
/*  Parallel Host Data Bus Width Selection
    0: 8-bit Parallel Host Data Bus.
    1: 16-bit Parallel Host Data Bus.*/
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x01);
  temp = ER5517.LCD_DataRead();
  temp |= cSetb0;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::RGB_16b_16bpp(void)
{
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x02);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb7;
  temp |= cSetb6;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::Graphic_Mode(void)
{
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x03);
  temp = ER5517.LCD_DataRead();
    temp &= cClrb2;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::Memory_Select_SDRAM(void)
{
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x03);
  temp = ER5517.LCD_DataRead();
    temp &= cClrb1;
    temp &= cClrb0; // B
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::HSCAN_L_to_R(void)
{
/*  
Horizontal Scan Direction
0 : From Left to Right
1 : From Right to Left
PIP window will be disabled when HDIR set as 1.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x12);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb4;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::VSCAN_T_to_B(void)
{
/*  
Vertical Scan direction
0 : From Top to Bottom
1 : From bottom to Top
PIP window will be disabled when VDIR set as 1.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x12);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb3;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::PDATA_Set_RGB(void)
{
/*  
parallel PDATA[23:0] Output Sequence
000b : RGB.
001b : RBG.
010b : GRB.
011b : GBR.
100b : BRG.
101b : BGR.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x12);
  temp = ER5517.LCD_DataRead();
    temp &=0xf8;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::PCLK_Rising(void)   
{
/*
PCLK Inversion
0: PDAT, DE, HSYNC etc. Drive(/ change) at PCLK falling edge.
1: PDAT, DE, HSYNC etc. Drive(/ change) at PCLK rising edge.
*/
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x12);
  temp = ER5517.LCD_DataRead();
    temp &= cClrb7;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::PCLK_Falling(void)
{
/*
PCLK Inversion
0: PDAT, DE, HSYNC etc. Drive(/ change) at PCLK falling edge.
1: PDAT, DE, HSYNC etc. Drive(/ change) at PCLK rising edge.
*/
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x12);
  temp = ER5517.LCD_DataRead();
    temp |= cSetb7;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::HSYNC_Low_Active(void)
{
/*  
HSYNC Polarity
0 : Low active.
1 : High active.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x13);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb7;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::HSYNC_High_Active(void)
{
/*  
HSYNC Polarity
0 : Low active.
1 : High active.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x13);
  temp = ER5517.LCD_DataRead();   
  temp |= cSetb7;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::VSYNC_Low_Active(void)
{
/*  
VSYNC Polarity
0 : Low active.
1 : High active.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x13);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb6; 
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::VSYNC_High_Active(void)
{
/*  
VSYNC Polarity
0 : Low active.
1 : High active.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x13);
  temp = ER5517.LCD_DataRead();
  temp |= cSetb6;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::DE_Low_Active(void)
{
/*  
DE Polarity
0 : High active.
1 : Low active.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x13);
  temp = ER5517.LCD_DataRead();
    temp |= cSetb5;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::DE_High_Active(void)
{
/*  
DE Polarity
0 : High active.
1 : Low active.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x13);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb5;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::Set_PCLK(unsigned char val)
{
  if(val == 1)  ER5517.PCLK_Falling();
  else      ER5517.PCLK_Rising();
}

void ER5517Basic::Set_HSYNC_Active(unsigned char val)
{
  if(val == 1)  ER5517.HSYNC_High_Active();
  else      ER5517.HSYNC_Low_Active();
}

void ER5517Basic::Set_VSYNC_Active(unsigned char val)
{
  if(val == 1)  ER5517.VSYNC_High_Active();
  else      ER5517.VSYNC_Low_Active();
}

void ER5517Basic::Set_DE_Active(unsigned char val)
{
  if(val == 1)  ER5517.DE_High_Active();
  else      ER5517.DE_Low_Active();
}
void ER5517Basic::LCD_HorizontalWidth_VerticalHeight(unsigned short WX,unsigned short HY)
{
  unsigned char temp;
  if(WX<8)
    {
  ER5517.LCD_CmdWrite(0x14);
  ER5517.LCD_DataWrite(0x00);
    
  ER5517.LCD_CmdWrite(0x15);
  ER5517.LCD_DataWrite(WX);
    
    temp=HY-1;
  ER5517.LCD_CmdWrite(0x1A);
  ER5517.LCD_DataWrite(temp);
      
  temp=(HY-1)>>8;
  ER5517.LCD_CmdWrite(0x1B);
  ER5517.LCD_DataWrite(temp);
  }
  else
  {
    temp=(WX/8)-1;
  ER5517.LCD_CmdWrite(0x14);
  ER5517.LCD_DataWrite(temp);
    
    temp=WX%8;
  ER5517.LCD_CmdWrite(0x15);
  ER5517.LCD_DataWrite(temp);
    
    temp=HY-1;
  ER5517.LCD_CmdWrite(0x1A);
  ER5517.LCD_DataWrite(temp);
      
  temp=(HY-1)>>8;
  ER5517.LCD_CmdWrite(0x1B);
  ER5517.LCD_DataWrite(temp);
  }
}
//[16h][17h]=========================================================================
void ER5517Basic::LCD_Horizontal_Non_Display(unsigned short WX)
{
  unsigned char temp;
  if(WX<8)
  {
  ER5517.LCD_CmdWrite(0x16);
  ER5517.LCD_DataWrite(0x00);
    
  ER5517.LCD_CmdWrite(0x17);
  ER5517.LCD_DataWrite(WX);
  }
  else
  {
    temp=(WX/8)-1;
  ER5517.LCD_CmdWrite(0x16);
  ER5517.LCD_DataWrite(temp);
    
    temp=WX%8;
  ER5517.LCD_CmdWrite(0x17);
  ER5517.LCD_DataWrite(temp);
  } 
}
//[18h]=========================================================================
void ER5517Basic::LCD_HSYNC_Start_Position(unsigned short WX)
{
  unsigned char temp;
  if(WX<8)
  {
  ER5517.LCD_CmdWrite(0x18);
  ER5517.LCD_DataWrite(0x00);
  }
  else
  {
    temp=(WX/8)-1;
  ER5517.LCD_CmdWrite(0x18);
  ER5517.LCD_DataWrite(temp);  
  }
}
//[19h]=========================================================================
void ER5517Basic::LCD_HSYNC_Pulse_Width(unsigned short WX)
{
  unsigned char temp;
  if(WX<8)
  {
  ER5517.LCD_CmdWrite(0x19);
  ER5517.LCD_DataWrite(0x00);
  }
  else
  {
    temp=(WX/8)-1;
  ER5517.LCD_CmdWrite(0x19);
  ER5517.LCD_DataWrite(temp);  
  }
}
//[1Ch][1Dh]=========================================================================
void ER5517Basic::LCD_Vertical_Non_Display(unsigned short HY)
{
  unsigned char temp;
    temp=HY-1;
  ER5517.LCD_CmdWrite(0x1C);
  ER5517.LCD_DataWrite(temp);

  ER5517.LCD_CmdWrite(0x1D);
  ER5517.LCD_DataWrite(temp>>8);
}
//[1Eh]=========================================================================
void ER5517Basic::LCD_VSYNC_Start_Position(unsigned short HY)
{
  unsigned char temp;
    temp=HY-1;
  ER5517.LCD_CmdWrite(0x1E);
  ER5517.LCD_DataWrite(temp);
}
//[1Fh]=========================================================================
void ER5517Basic::LCD_VSYNC_Pulse_Width(unsigned short HY)
{
  unsigned char temp;
    temp=HY-1;
  ER5517.LCD_CmdWrite(0x1F);
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::Memory_XY_Mode(void) 
{
  unsigned char temp;

  ER5517.LCD_CmdWrite(0x5E);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb2;
  ER5517.LCD_DataWrite(temp);
}

void ER5517Basic::Memory_16bpp_Mode(void)  
{
  unsigned char temp;

  ER5517.LCD_CmdWrite(0x5E);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb1;
  temp |= cSetb0;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::Select_Main_Window_16bpp(void)
{
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x10);
  temp = ER5517.LCD_DataRead();
    temp &= cClrb3;
    temp |= cSetb2;
  ER5517.LCD_DataWrite(temp);
}
void ER5517Basic::Main_Image_Start_Address(unsigned long Addr) 
{
/*
[20h] Main Image Start Address[7:2]
[21h] Main Image Start Address[15:8]
[22h] Main Image Start Address [23:16]
[23h] Main Image Start Address [31:24]
*/
  ER5517.LCD_RegisterWrite(0x20,Addr);
  ER5517.LCD_RegisterWrite(0x21,Addr>>8);
  ER5517.LCD_RegisterWrite(0x22,Addr>>16);
  ER5517.LCD_RegisterWrite(0x23,Addr>>24);
}
void ER5517Basic::Main_Image_Width(unsigned short WX)  
{
/*
[24h] Main Image Width [7:0]
[25h] Main Image Width [12:8]
Unit: Pixel.
It must be divisible by 4. MIW Bit [1:0] tie to ��0�� internally.
The value is physical pixel number. Maximum value is 8188 pixels
*/
  ER5517.LCD_RegisterWrite(0x24,WX);
  ER5517.LCD_RegisterWrite(0x25,WX>>8);
}
//[26h][27h][28h][29h]=========================================================================
void ER5517Basic::Main_Window_Start_XY(unsigned short WX,unsigned short HY)  
{
/*
[26h] Main Window Upper-Left corner X-coordination [7:0]
[27h] Main Window Upper-Left corner X-coordination [12:8]
Reference Main Image coordination.
Unit: Pixel
It must be divisible by 4. MWULX Bit [1:0] tie to ��0�� internally.
X-axis coordination plus Horizontal display width cannot large than 8188.

[28h] Main Window Upper-Left corner Y-coordination [7:0]
[29h] Main Window Upper-Left corner Y-coordination [12:8]
Reference Main Image coordination.
Unit: Pixel
Range is between 0 and 8191.
*/
  ER5517.LCD_RegisterWrite(0x26,WX);
  ER5517.LCD_RegisterWrite(0x27,WX>>8);

  ER5517.LCD_RegisterWrite(0x28,HY);
  ER5517.LCD_RegisterWrite(0x29,HY>>8);
}
void ER5517Basic::Canvas_Image_Start_address(unsigned long Addr) 
{
/*
[50h] Start address of Canvas [7:0]
[51h] Start address of Canvas [15:8]
[52h] Start address of Canvas [23:16]
[53h] Start address of Canvas [31:24]
*/
  ER5517.LCD_RegisterWrite(0x50,Addr);
  ER5517.LCD_RegisterWrite(0x51,Addr>>8);
  ER5517.LCD_RegisterWrite(0x52,Addr>>16);
  ER5517.LCD_RegisterWrite(0x53,Addr>>24);
}
//[54h][55h]=========================================================================
void ER5517Basic::Canvas_image_width(unsigned short WX)  
{
/*
[54h] Canvas image width [7:2]
[55h] Canvas image width [12:8]
*/
  ER5517.LCD_RegisterWrite(0x54,WX);
  ER5517.LCD_RegisterWrite(0x55,WX>>8);
}
//[56h][57h][58h][59h]=========================================================================
void ER5517Basic::Active_Window_XY(unsigned short WX,unsigned short HY)  
{
/*
[56h] Active Window Upper-Left corner X-coordination [7:0]
[57h] Active Window Upper-Left corner X-coordination [12:8]
[58h] Active Window Upper-Left corner Y-coordination [7:0]
[59h] Active Window Upper-Left corner Y-coordination [12:8]
*/
  ER5517.LCD_RegisterWrite(0x56,WX);
  ER5517.LCD_RegisterWrite(0x57,WX>>8);
  
  ER5517.LCD_RegisterWrite(0x58,HY);
  ER5517.LCD_RegisterWrite(0x59,HY>>8);
}
//[5Ah][5Bh][5Ch][5Dh]=========================================================================
void ER5517Basic::Active_Window_WH(unsigned short WX,unsigned short HY)  
{
/*
[5Ah] Width of Active Window [7:0]
[5Bh] Width of Active Window [12:8]
[5Ch] Height of Active Window [7:0]
[5Dh] Height of Active Window [12:8]
*/
  ER5517.LCD_RegisterWrite(0x5A,WX);
  ER5517.LCD_RegisterWrite(0x5B,WX>>8);
 
  ER5517.LCD_RegisterWrite(0x5C,HY);
  ER5517.LCD_RegisterWrite(0x5D,HY>>8);
}
void ER5517Basic::Foreground_color_65k(unsigned short temp)
{
    ER5517.LCD_CmdWrite(0xD2);
  ER5517.LCD_DataWrite(temp>>8);
 
    ER5517.LCD_CmdWrite(0xD3);
  ER5517.LCD_DataWrite(temp>>3);
  
    ER5517.LCD_CmdWrite(0xD4);
  ER5517.LCD_DataWrite(temp<<3);
}

//Input data format:R5G6B6

void ER5517Basic::Check_Busy_Draw(void)
{
  unsigned char temp;
  do
  {
    temp=ER5517.LCD_StatusRead();
  }
  while(temp&0x08);

}

void ER5517Basic::Check_2D_Busy(void)
{
  do
  {
    
  }
  while( ER5517.LCD_StatusRead()&0x08 );
}

void ER5517Basic::DrawSquare_Fill
(
 unsigned short X1                
,unsigned short Y1              
,unsigned short X2                
,unsigned short Y2              
,unsigned long ForegroundColor   
)
{
  ER5517.Foreground_color_65k(ForegroundColor);
  ER5517.Square_Start_XY(X1,Y1);
  ER5517.Square_End_XY(X2,Y2);
  ER5517.Start_Square_Fill();
  ER5517.Check_2D_Busy();
}

//[67h]=========================================================================
/*
[bit7]Draw Line / Triangle Start Signal
Write Function
0 : Stop the drawing function.
1 : Start the drawing function.
Read Function
0 : Drawing function complete.
1 : Drawing function is processing.
[bit5]Fill function for Triangle Signal
0 : Non fill.
1 : Fill.
[bit1]Draw Triangle or Line Select Signal
0 : Draw Line
1 : Draw Triangle
*/
//[68h][69h][6Ah][6Bh]=========================================================================
//[6Ch][6Dh][6Eh][6Fh]=========================================================================
//���յ�
//[68h]~[73h]=========================================================================
//�T��-�I1
//�T��-�I2
//�T��-�I3

void ER5517Basic::Square_Start_XY(unsigned short WX,unsigned short HY)
{
/*
[68h] Draw Line/Square/Triangle Start X-coordination [7:0]
[69h] Draw Line/Square/Triangle Start X-coordination [12:8]
[6Ah] Draw Line/Square/Triangle Start Y-coordination [7:0]
[6Bh] Draw Line/Square/Triangle Start Y-coordination [12:8]
*/
  ER5517.LCD_CmdWrite(0x68);
  ER5517.LCD_DataWrite(WX);

  ER5517.LCD_CmdWrite(0x69);
  ER5517.LCD_DataWrite(WX>>8);

  ER5517.LCD_CmdWrite(0x6A);
  ER5517.LCD_DataWrite(HY);

  ER5517.LCD_CmdWrite(0x6B);
  ER5517.LCD_DataWrite(HY>>8);
}

void ER5517Basic::Square_End_XY(unsigned short WX,unsigned short HY)
{
/*
[6Ch] Draw Line/Square/Triangle End X-coordination [7:0]
[6Dh] Draw Line/Square/Triangle End X-coordination [12:8]
[6Eh] Draw Line/Square/Triangle End Y-coordination [7:0]
[6Fh] Draw Line/Square/Triangle End Y-coordination [12:8]
*/
  ER5517.LCD_CmdWrite(0x6C);
  ER5517.LCD_DataWrite(WX);

  ER5517.LCD_CmdWrite(0x6D);
  ER5517.LCD_DataWrite(WX>>8);

  ER5517.LCD_CmdWrite(0x6E);
  ER5517.LCD_DataWrite(HY);

  ER5517.LCD_CmdWrite(0x6F);
  ER5517.LCD_DataWrite(HY>>8);
}
//[76h]=========================================================================
/*
[bit7]
Draw Circle / Ellipse / Square /Circle Square Start Signal 
Write Function
0 : Stop the drawing function.
1 : Start the drawing function.
Read Function
0 : Drawing function complete.
1 : Drawing function is processing.
[bit6]
Fill the Circle / Ellipse / Square / Circle Square Signal
0 : Non fill.
1 : fill.
[bit5 bit4]
Draw Circle / Ellipse / Square / Ellipse Curve / Circle Square Select
00 : Draw Circle / Ellipse
01 : Draw Circle / Ellipse Curve
10 : Draw Square.
11 : Draw Circle Square.
[bit1 bit0]
Draw Circle / Ellipse Curve Part Select
00 : 
01 : 
10 : 
11 : 
*/
//
//
//
void ER5517Basic::Start_Square_Fill(void)
{
  ER5517.LCD_CmdWrite(0x76);
  ER5517.LCD_DataWrite(0xE0);//B1110_XXXX
  Check_Busy_Draw();
}
//[77h]~[7Eh]=========================================================================

//[84h]=========================================================================
//[85h]=========================================================================
//[85h].[bit3][bit2]=========================================================================
/*
XPWM[1] pin function control
0X: XPWM[1] output system error flag (REG[00h] bit[1:0], Scan bandwidth insufficient + Memory access out of range)
10: XPWM[1] enabled and controlled by PWM timer 1
11: XPWM[1] output oscillator clock
//If XTEST[0] set high, then XPWM[1] will become panel scan clock input.
*/
//[85h].[bit1][bit0]=========================================================================
/*
XPWM[0] pin function control
0X: XPWM[0] becomes GPIO-C[7]
10: XPWM[0] enabled and controlled by PWM timer 0
11: XPWM[0] output core clock
*/
//[86h]=========================================================================
//[86h]PWM1
//[86h]PWM0
//[87h]=========================================================================
//[88h][89h]=========================================================================
//[8Ah][8Bh]=========================================================================
//[8Ch][8Dh]=========================================================================
//[8Eh][8Fh]=========================================================================

//[90h]~[B5h]=========================================================================

//[90h]=========================================================================
void ER5517Basic::BTE_Enable(void)
{ 
/*
BTE Function Enable
0 : BTE Function disable.
1 : BTE Function enable.
*/
    unsigned char temp;
    ER5517.LCD_CmdWrite(0x90);
    temp = ER5517.LCD_DataRead();
    temp |= cSetb4 ;
  ER5517.LCD_DataWrite(temp);  
}

//[90h]=========================================================================

//[90h]=========================================================================
void ER5517Basic::Check_BTE_Busy(void)
{ 
/*
BTE Function Status
0 : BTE Function is idle.
1 : BTE Function is busy.
*/
  unsigned char temp;   
  do
  {
    temp=ER5517.LCD_StatusRead();
  }while(temp&0x08);

}
//[90h]=========================================================================
 
//[90h]=========================================================================
 

//[91h]=========================================================================
void ER5517Basic::BTE_ROP_Code(unsigned char setx)
{ 
/*
BTE ROP Code[Bit7:4]
  
0000 : 0(Blackness)
0001 : ~S0.~S1 or ~ ( S0+S1 )
0010 : ~S0.S1
0011 : ~S0
0100 : S0.~S1
0101 : ~S1
0110 : S0^S1
0111 : ~S0+~S1 or ~ ( S0.S1 )
1000 : S0.S1
1001 : ~ ( S0^S1 )
1010 : S1
1011 : ~S0+S1
1100 : S0
1101 : S0+~S1
1110 : S0+S1
1111 : 1 ( Whiteness )
*/
    unsigned char temp;
    ER5517.LCD_CmdWrite(0x91);
    temp = ER5517.LCD_DataRead();
    temp &= 0x0f ;
    temp |= (setx<<4);
    ER5517.LCD_DataWrite(temp);
}
  
//[91h]=========================================================================
void ER5517Basic::BTE_Operation_Code(unsigned char setx)
{ 
/*
BTE Operation Code[Bit3:0]
  
0000 : MPU Write BTE with ROP.
0001 : MPU Read BTE w/o ROP.
0010 : Memory copy (move) BTE in positive direction with ROP.
0011 : Memory copy (move) BTE in negative direction with ROP.
0100 : MPU Transparent Write BTE. (w/o ROP.)
0101 : Transparent Memory copy (move) BTE in positive direction (w/o ROP.)
0110 : Pattern Fill with ROP.
0111 : Pattern Fill with key-chroma
1000 : Color Expansion
1001 : Color Expansion with transparency
1010 : Move BTE in positive direction with Alpha blending
1011 : MPU Write BTE with Alpha blending
1100 : Solid Fill
1101 : Reserved
1110 : Reserved
1111 : Reserved
*/
    unsigned char temp;
    ER5517.LCD_CmdWrite(0x91);
    temp = ER5517.LCD_DataRead();
    temp &= 0xf0 ;
    temp |= setx ;
    ER5517.LCD_DataWrite(temp);

}
//[92h]=========================================================================
 
//[92h]=========================================================================
void ER5517Basic::BTE_S0_Color_16bpp(void)
{ 
/*
S0 Color Depth
00 : 256 Color
01 : 64k Color
1x : 16M Color
*/  
    unsigned char temp;
    ER5517.LCD_CmdWrite(0x92);
    temp = ER5517.LCD_DataRead();
    temp &= cClrb6 ;
    temp |= cSetb5 ;
    ER5517.LCD_DataWrite(temp);

} 
//[92h]=========================================================================
//[92h]=========================================================================
 
//[92h]=========================================================================
void ER5517Basic::BTE_S1_Color_16bpp(void)
{ 
/*
S1 Color Depth
000 : 256 Color
001 : 64k Color
010 : 16M Color
011 : Constant Color
100 : 8 bit pixel alpha blending
101 : 16 bit pixel alpha blending
*/  
    unsigned char temp;
    ER5517.LCD_CmdWrite(0x92);
    temp = ER5517.LCD_DataRead();
    temp &= cClrb4 ;
    temp &= cClrb3 ;
    temp |= cSetb2 ;
    ER5517.LCD_DataWrite(temp);

}
//[92h]=========================================================================

//[92h]=========================================================================

//[92h]=========================================================================

//[92h]=========================================================================

//[92h]=========================================================================
 
//[92h]=========================================================================
void ER5517Basic::BTE_Destination_Color_16bpp(void)
{ 
/*
Destination Color Depth
00 : 256 Color
01 : 64k Color
1x : 16M Color
*/  
    unsigned char temp;
    ER5517.LCD_CmdWrite(0x92);
    temp = ER5517.LCD_DataRead();
    temp &= cClrb1 ;
    temp |= cSetb0 ;
    ER5517.LCD_DataWrite(temp);

} 
//[92h]=========================================================================

//[93h][94h][95h][96h]=========================================================================
void ER5517Basic::BTE_S0_Memory_Start_Address(unsigned long Addr)  
{
/*
[93h] BTE S0 Memory Start Address [7:0]
[94h] BTE S0 Memory Start Address [15:8]
[95h] BTE S0 Memory Start Address [23:16]
[96h] BTE S0 Memory Start Address [31:24]
Bit [1:0] tie to ��0�� internally.
*/
  ER5517.LCD_RegisterWrite(0x93,Addr);
  ER5517.LCD_RegisterWrite(0x94,Addr>>8);
  ER5517.LCD_RegisterWrite(0x95,Addr>>16);
  ER5517.LCD_RegisterWrite(0x96,Addr>>24);
}

//[97h][98h]=========================================================================
void ER5517Basic::BTE_S0_Image_Width(unsigned short WX)  
{
/*
[97h] BTE S0 Image Width [7:0]
[98h] BTE S0 Image Width [12:8]
Unit: Pixel.
Bit [1:0] tie to ��0�� internally.
*/
  ER5517.LCD_RegisterWrite(0x97,WX);
  ER5517.LCD_RegisterWrite(0x98,WX>>8);
}

//[99h][9Ah][9Bh][9Ch]=========================================================================
void ER5517Basic::BTE_S0_Window_Start_XY(unsigned short WX,unsigned short HY)  
{
/*
[99h] BTE S0 Window Upper-Left corner X-coordination [7:0]
[9Ah] BTE S0 Window Upper-Left corner X-coordination [12:8]
[9Bh] BTE S0 Window Upper-Left corner Y-coordination [7:0]
[9Ch] BTE S0 Window Upper-Left corner Y-coordination [12:8]
*/
  ER5517.LCD_RegisterWrite(0x99,WX);
  ER5517.LCD_RegisterWrite(0x9A,WX>>8);

  ER5517.LCD_RegisterWrite(0x9B,HY);
  ER5517.LCD_RegisterWrite(0x9C,HY>>8);
}

//[9Dh][9Eh][9Fh][A0h]=========================================================================
void ER5517Basic::BTE_S1_Memory_Start_Address(unsigned long Addr)  
{
/*
[9Dh] BTE S1 Memory Start Address [7:0]
[9Eh] BTE S1 Memory Start Address [15:8]
[9Fh] BTE S1 Memory Start Address [23:16]
[A0h] BTE S1 Memory Start Address [31:24]
Bit [1:0] tie to ��0�� internally.
*/
  ER5517.LCD_RegisterWrite(0x9D,Addr);
  ER5517.LCD_RegisterWrite(0x9E,Addr>>8);
  ER5517.LCD_RegisterWrite(0x9F,Addr>>16);
  ER5517.LCD_RegisterWrite(0xA0,Addr>>24);
}

//Input data format:R3G3B2

//Input data format:R5G6B6

//Input data format:R8G8B8

//[A1h][A2h]=========================================================================
void ER5517Basic::BTE_S1_Image_Width(unsigned short WX)  
{
/*
[A1h] BTE S1 Image Width [7:0]
[A2h] BTE S1 Image Width [12:8]
Unit: Pixel.
Bit [1:0] tie to ��0�� internally.
*/
  ER5517.LCD_RegisterWrite(0xA1,WX);
  ER5517.LCD_RegisterWrite(0xA2,WX>>8);
}

//[A3h][A4h][A5h][A6h]=========================================================================
void ER5517Basic::BTE_S1_Window_Start_XY(unsigned short WX,unsigned short HY)  
{
/*
[A3h] BTE S1 Window Upper-Left corner X-coordination [7:0]
[A4h] BTE S1 Window Upper-Left corner X-coordination [12:8]
[A5h] BTE S1 Window Upper-Left corner Y-coordination [7:0]
[A6h] BTE S1 Window Upper-Left corner Y-coordination [12:8]
*/
  ER5517.LCD_RegisterWrite(0xA3,WX);
  ER5517.LCD_RegisterWrite(0xA4,WX>>8);

  ER5517.LCD_RegisterWrite(0xA5,HY);
  ER5517.LCD_RegisterWrite(0xA6,HY>>8);
}

//[A7h][A8h][A9h][AAh]=========================================================================
void ER5517Basic::BTE_Destination_Memory_Start_Address(unsigned long Addr) 
{
/*
[A7h] BTE Destination Memory Start Address [7:0]
[A8h] BTE Destination Memory Start Address [15:8]
[A9h] BTE Destination Memory Start Address [23:16]
[AAh] BTE Destination Memory Start Address [31:24]
Bit [1:0] tie to ��0�� internally.
*/
  ER5517.LCD_RegisterWrite(0xA7,Addr);
  ER5517.LCD_RegisterWrite(0xA8,Addr>>8);
  ER5517.LCD_RegisterWrite(0xA9,Addr>>16);
  ER5517.LCD_RegisterWrite(0xAA,Addr>>24);
}

//[ABh][ACh]=========================================================================
void ER5517Basic::BTE_Destination_Image_Width(unsigned short WX) 
{
/*
[ABh] BTE Destination Image Width [7:0]
[ACh] BTE Destination Image Width [12:8]
Unit: Pixel.
Bit [1:0] tie to ��0�� internally.
*/
  ER5517.LCD_RegisterWrite(0xAB,WX);
  ER5517.LCD_RegisterWrite(0xAC,WX>>8);
}

//[ADh][AEh][AFh][B0h]=========================================================================
void ER5517Basic::BTE_Destination_Window_Start_XY(unsigned short WX,unsigned short HY) 
{
/*
[ADh] BTE Destination Window Upper-Left corner X-coordination [7:0]
[AEh] BTE Destination Window Upper-Left corner X-coordination [12:8]
[AFh] BTE Destination Window Upper-Left corner Y-coordination [7:0]
[B0h] BTE Destination Window Upper-Left corner Y-coordination [12:8]
*/
  ER5517.LCD_RegisterWrite(0xAD,WX);
  ER5517.LCD_RegisterWrite(0xAE,WX>>8);

  ER5517.LCD_RegisterWrite(0xAF,HY);
  ER5517.LCD_RegisterWrite(0xB0,HY>>8);
}

//[B1h][B2h][B3h][B4h]===============================================================

void ER5517Basic::BTE_Window_Size(unsigned short WX, unsigned short WY)

{
/*
[B1h] BTE Window Width [7:0]
[B2h] BTE Window Width [12:8]

[B3h] BTE Window Height [7:0]
[B4h] BTE Window Height [12:8]
*/
        ER5517.LCD_RegisterWrite(0xB1,WX);
        ER5517.LCD_RegisterWrite(0xB2,WX>>8);
  
      ER5517.LCD_RegisterWrite(0xB3,WY);
        ER5517.LCD_RegisterWrite(0xB4,WY>>8);
}

//[B5h]=========================================================================

//[B6h]=========================================================================

//[B7h]=========================================================================

//REG[B8h] SPI master Tx /Rx FIFO Data Register (SPIDR) 

//REG[B9h] SPI master Control Register (SPIMCR2) 
 

//0: inactive (nSS port will goes high) 
//1: active (nSS port will goes low) 

//Interrupt enable for FIFO overflow error [OVFIRQEN] 
//Interrupt enable for while Tx FIFO empty & SPI engine/FSM idle

//At CPOL=0 the base value of the clock is zero   
//o  For CPHA=0, data are read on the clock's rising edge (low->high transition) and 
//data are changed on a falling edge (high->low clock transition). 
//o  For CPHA=1, data are read on the clock's falling edge and data are changed on a 
//rising edge. 

//At CPOL=1 the base value of the clock is one (inversion of CPOL=0)   
//o  For CPHA=0, data are read on clock's falling edge and data are changed on a 
//rising edge. 
//o  For CPHA=1, data are read on clock's rising edge and data are changed on a 
//falling edge.

//REG[BAh] SPI master Status Register (SPIMSR) 

 

 

 

//REG[BB] SPI Clock period (SPIDIV) 
 

//[BCh][BDh][BEh][BFh]=========================================================================
//[C0h][C1h][C2h][C3h]=========================================================================
//[C0h][C1h][C2h][C3h]=========================================================================

//[C6h][C7h][C8h][C9h]=========================================================================
//[CAh][CBh]=========================================================================

//[CCh]=========================================================================

//[CDh]=========================================================================

//[D0h]=========================================================================
//[D1h]=========================================================================

//[DBh]~[DEh]=========================================================================

void ER5517Basic::MemWrite_Left_Right_Top_Down(void)
{
  unsigned char temp;
  ER5517.LCD_CmdWrite(0x02);
  temp = ER5517.LCD_DataRead();
  temp &= cClrb2;
  temp &= cClrb1;
  ER5517.LCD_DataWrite(temp);
}

void ER5517Basic::System_Check_Temp(void)
{
  unsigned char i=0,j=0;
  unsigned char temp=0;
  unsigned char system_ok=0;
  do
  {
    j = ER5517.LCD_StatusRead();
    if((j&0x02)==0x00)    
    {
      delay(2);                  //MCU too fast, necessary
      ER5517.LCD_CmdWrite(0x01);
      delay(2);                  //MCU too fast, necessary
      temp = ER5517.LCD_DataRead();
      if((temp & 0x80) == 0x80)       //Check CCR register's PLL is ready or not
      {
        system_ok=1;
        i=0;
      }
      else
      {
        delay(2); //MCU too fast, necessary
        ER5517.LCD_CmdWrite(0x01);
        delay(2); //MCU too fast, necessary
        ER5517.LCD_DataWrite(0x80);
      }
    }
    else
    {
      system_ok=0;
      i++;
    }
    if(system_ok==0 && i==5)
    {
      ER5517.HW_Reset(); //note1
      i=0;
    }
  }while(system_ok==0);
}

void ER5517Basic::PLL_Initial(void) 
{
     /*
      unsigned char CCLK = 0;
      unsigned char MCLK = 0;
      unsigned char SCLKP = 0;	

       long temp = 0;
	long temp1 = 0;
	long temp2 = 0;
	long temp3 = 0;
	
	unsigned short lpllOD_sclk, lpllOD_cclk, lpllOD_mclk;
	unsigned short lpllR_sclk, lpllR_cclk, lpllR_mclk;
	unsigned short lpllN_sclk, lpllN_cclk, lpllN_mclk;
	
	
	//temp = (LCD_HBPD + LCD_HFPD + LCD_HSPW + LCD_XSIZE_TFT) * (LCD_VBPD + LCD_VFPD + LCD_VSPW+LCD_YSIZE_TFT) * 60;   
	temp1 = LCD_HBPD + LCD_HFPD + LCD_HSPW + LCD_XSIZE_TFT;
	temp2 = LCD_VBPD + LCD_VFPD + LCD_VSPW+LCD_YSIZE_TFT;
	temp = temp1 * temp2 * 68;
	

	temp3 = ((temp%1000000)/100000);
	if(temp3>=5)
		 temp = temp / 1000000 + 1;
	else temp = temp / 1000000;
	
	SCLKP = temp;
	temp = temp * 3;
	MCLK = temp;
	CCLK = temp;

	
	if(CCLK > 100)	CCLK = 100;
	if(MCLK > 100)	MCLK = 100;
	if(SCLKP > 65)	SCLKP = 65;

////// XI_10M 	
	
	lpllOD_sclk = 3;
	lpllOD_cclk = 2;
	lpllOD_mclk = 2;
	lpllR_sclk  = 5;
	lpllR_cclk  = 5;
	lpllR_mclk  = 5;
	lpllN_mclk  = MCLK;      
	lpllN_cclk  = CCLK;    
	lpllN_sclk  = 2*SCLKP; 

	

	ER5517.LCD_CmdWrite(0x05);
	ER5517.LCD_DataWrite((lpllOD_sclk<<6) | (lpllR_sclk<<1) | ((lpllN_sclk>>8)&0x1));
	ER5517.LCD_CmdWrite(0x07);
	ER5517.LCD_DataWrite((lpllOD_mclk<<6) | (lpllR_mclk<<1) | ((lpllN_mclk>>8)&0x1));
	ER5517.LCD_CmdWrite(0x09);
	ER5517.LCD_DataWrite((lpllOD_cclk<<6) | (lpllR_cclk<<1) | ((lpllN_cclk>>8)&0x1));

	ER5517.LCD_CmdWrite(0x06);
	ER5517.LCD_DataWrite(lpllN_sclk);
	ER5517.LCD_CmdWrite(0x08);
	ER5517.LCD_DataWrite(lpllN_mclk);
	ER5517.LCD_CmdWrite(0x0a);
	ER5517.LCD_DataWrite(lpllN_cclk);
      
	ER5517.LCD_CmdWrite(0x00);
	delayMicroseconds(1);
	ER5517.LCD_DataWrite(0x80);

	delay(1);
  */
  
 	unsigned short lpllOD_sclk, lpllOD_cclk, lpllOD_mclk;
	unsigned short lpllR_sclk, lpllR_cclk, lpllR_mclk;
	unsigned short lpllN_sclk, lpllN_cclk, lpllN_mclk;

	//Fout = Fin*(N/R)/OD
	//Fout = 10*N/(2*5) = N
	lpllOD_sclk = 2;
	lpllOD_cclk = 2;
	lpllOD_mclk = 2;
	lpllR_sclk  = 5;
	lpllR_cclk  = 5;
	lpllR_mclk  = 5;
	lpllN_sclk  = 75;   //  frequency
	lpllN_cclk  = 100;    // Core CLK:100
	lpllN_mclk  = 100;    // SRAM CLK:100
	  
	ER5517.LCD_CmdWrite(0x05);
	ER5517.LCD_DataWrite((lpllOD_sclk<<6) | (lpllR_sclk<<1) | ((lpllN_sclk>>8)&0x1));
	ER5517.LCD_CmdWrite(0x07);
	ER5517.LCD_DataWrite((lpllOD_mclk<<6) | (lpllR_mclk<<1) | ((lpllN_mclk>>8)&0x1));
	ER5517.LCD_CmdWrite(0x09);
	ER5517.LCD_DataWrite((lpllOD_cclk<<6) | (lpllR_cclk<<1) | ((lpllN_cclk>>8)&0x1));

	ER5517.LCD_CmdWrite(0x06);
	ER5517.LCD_DataWrite(lpllN_sclk);
	ER5517.LCD_CmdWrite(0x08);
	ER5517.LCD_DataWrite(lpllN_mclk);
	ER5517.LCD_CmdWrite(0x0a);
	ER5517.LCD_DataWrite(lpllN_cclk);

	ER5517.LCD_CmdWrite(0x00);
	delayMicroseconds(1);
	ER5517.LCD_DataWrite(0x80);
	delay(1);

/*

      // ==== [SW_(1)]  PLL
    #define OSC_FREQ     10	  // crystal clcok
    #define DRAM_FREQ    100  // SDRAM clock frequency, unti: MHz		  
    #define CORE_FREQ    100  // Core (system) clock frequency, unit: MHz 
    #define SCAN_FREQ     10 // Panel Scan clock frequency, unit: MHz	 

 // Set pixel clock
  if(SCAN_FREQ>=63)        //&&(SCAN_FREQ<=100))
  {
	ER5517.LCD_CmdWrite(0x05);    //PLL Divided by 4
	ER5517.LCD_DataWrite(0x04);
	ER5517.LCD_CmdWrite(0x06);
	ER5517.LCD_DataWrite((SCAN_FREQ*4/OSC_FREQ)-1);
  }
  if((SCAN_FREQ>=32)&&(SCAN_FREQ<=62))
  {           
	ER5517.LCD_CmdWrite(0x05);    //PLL Divided by 8
	ER5517.LCD_DataWrite(0x06);
	ER5517.LCD_CmdWrite(0x06);
	ER5517.LCD_DataWrite((SCAN_FREQ*8/OSC_FREQ)-1);
  }
  if((SCAN_FREQ>=16)&&(SCAN_FREQ<=31))
  {           
	ER5517.LCD_CmdWrite(0x05);    //PLL Divided by 16
	ER5517.LCD_DataWrite(0x16);
	ER5517.LCD_CmdWrite(0x06);
	ER5517.LCD_DataWrite((SCAN_FREQ*16/OSC_FREQ)-1);
  }
  if((SCAN_FREQ>=8)&&(SCAN_FREQ<=15))
  {
	ER5517.LCD_CmdWrite(0x05);    //PLL Divided by 32
	ER5517.LCD_DataWrite(0x26);
	ER5517.LCD_CmdWrite(0x06);
	ER5517.LCD_DataWrite((SCAN_FREQ*32/OSC_FREQ)-1);
  }
  if((SCAN_FREQ>0)&&(SCAN_FREQ<=7))
  {
	ER5517.LCD_CmdWrite(0x05);    //PLL Divided by 64
	ER5517.LCD_DataWrite(0x36);
	ER5517.LCD_CmdWrite(0x06);
	ER5517.LCD_DataWrite((SCAN_FREQ*64/OSC_FREQ)-1);
  }            
 
  
  // Set SDRAM clock
  if(DRAM_FREQ>=125)        //&&(DRAM_FREQ<=166))
  {
	ER5517.LCD_CmdWrite(0x07);    //PLL Divided by 2
	ER5517.LCD_DataWrite(0x02);
	ER5517.LCD_CmdWrite(0x08);
	ER5517.LCD_DataWrite((DRAM_FREQ*2/OSC_FREQ)-1);
  }
  if((DRAM_FREQ>=63)&&(DRAM_FREQ<=124))   //&&(DRAM_FREQ<=166)
  {
	ER5517.LCD_CmdWrite(0x07);    //PLL Divided by 4
	ER5517.LCD_DataWrite(0x04);
	ER5517.LCD_CmdWrite(0x08);
	ER5517.LCD_DataWrite((DRAM_FREQ*4/OSC_FREQ)-1);
  }
  if((DRAM_FREQ>=31)&&(DRAM_FREQ<=62))
  {           
	ER5517.LCD_CmdWrite(0x07);    //PLL Divided by 8
	ER5517.LCD_DataWrite(0x06);
	ER5517.LCD_CmdWrite(0x08);
	ER5517.LCD_DataWrite((DRAM_FREQ*8/OSC_FREQ)-1);
  }
  if(DRAM_FREQ<=30)
  {
	ER5517.LCD_CmdWrite(0x07);    //PLL Divided by 8
	ER5517.LCD_DataWrite(0x06);
	ER5517.LCD_CmdWrite(0x08); //
	ER5517.LCD_DataWrite((30*8/OSC_FREQ)-1);
  }
 

  // Set Core clock
  if(CORE_FREQ>=125)
  {
	ER5517.LCD_CmdWrite(0x09);    //PLL Divided by 2
	ER5517.LCD_DataWrite(0x02);
	ER5517.LCD_CmdWrite(0x0A);
	ER5517.LCD_DataWrite((CORE_FREQ*2/OSC_FREQ)-1);
  }
  if((CORE_FREQ>=63)&&(CORE_FREQ<=124))     
  {
	ER5517.LCD_CmdWrite(0x09);    //PLL Divided by 4
	ER5517.LCD_DataWrite(0x04);
	ER5517.LCD_CmdWrite(0x0A);
	ER5517.LCD_DataWrite((CORE_FREQ*4/OSC_FREQ)-1);
  }
  if((CORE_FREQ>=31)&&(CORE_FREQ<=62))
  {           
	ER5517.LCD_CmdWrite(0x09);    //PLL Divided by 8
	ER5517.LCD_DataWrite(0x06);
	ER5517.LCD_CmdWrite(0x0A);
	ER5517.LCD_DataWrite((CORE_FREQ*8/OSC_FREQ)-1);
  }
  if(CORE_FREQ<=30)
  {
	ER5517.LCD_CmdWrite(0x09);    //PLL Divided by 8
	ER5517.LCD_DataWrite(0x06);
	ER5517.LCD_CmdWrite(0x0A); // 
	ER5517.LCD_DataWrite((30*8/OSC_FREQ)-1);
  }

	ER5517.LCD_CmdWrite(0x01);
	ER5517.LCD_CmdWrite(0x00);
	delay(1);
	ER5517.LCD_CmdWrite(0x80);
	//Enable_PLL();

	delay(1);	//
 */
}

void ER5517Basic::SDRAM_initail(void)
{
  unsigned short sdram_itv;
  
  ER5517.LCD_RegisterWrite(0xe0,0x29);      
  ER5517.LCD_RegisterWrite(0xe1,0x03); //CAS:2=0x02�ACAS:3=0x03
  sdram_itv = (64000000 / 8192) / (1000/60) ;
  sdram_itv-=2;

  ER5517.LCD_RegisterWrite(0xe2,sdram_itv);
  ER5517.LCD_RegisterWrite(0xe3,sdram_itv >>8);
  ER5517.LCD_RegisterWrite(0xe4,0x01);
  ER5517.Check_SDRAM_Ready();
  delay(1);
}
void ER5517Basic::HW_Reset(void)
{
	pinMode(LCM_RESET, OUTPUT);
  digitalWrite(LCM_RESET, LOW);
  delay(50);
  digitalWrite(LCM_RESET, HIGH);
  delay(100);
}

void ER5517Basic::initial(void)
{

    ER5517.PLL_Initial();
  
    ER5517.SDRAM_initail();

//**[01h]**//
    ER5517.TFT_24bit();
  ER5517.Host_Bus_16bit(); //Host bus 16bit
      
//**[02h]**//
  ER5517.RGB_16b_16bpp();
  ER5517.MemWrite_Left_Right_Top_Down(); 
      
//**[03h]**//
  ER5517.Graphic_Mode();
  ER5517.Memory_Select_SDRAM();

  ER5517.HSCAN_L_to_R();     //REG[12h]:from left to right
  ER5517.VSCAN_T_to_B();       //REG[12h]:from top to bottom
  ER5517.PDATA_Set_RGB();        //REG[12h]:Select RGB output

  ER5517.Set_PCLK(LCD_PCLK_Falling_Rising);   //LCD_PCLK_Falling_Rising
  ER5517.Set_HSYNC_Active(LCD_HSYNC_Active_Polarity);
  ER5517.Set_VSYNC_Active(LCD_VSYNC_Active_Polarity);
  ER5517.Set_DE_Active(LCD_DE_Active_Polarity);
 
  ER5517.LCD_HorizontalWidth_VerticalHeight(LCD_XSIZE_TFT ,LCD_YSIZE_TFT);
  ER5517.LCD_Horizontal_Non_Display(LCD_HBPD);                          
  ER5517.LCD_HSYNC_Start_Position(LCD_HFPD);                              
  ER5517.LCD_HSYNC_Pulse_Width(LCD_HSPW);                              
  ER5517.LCD_Vertical_Non_Display(LCD_VBPD);                               
  ER5517.LCD_VSYNC_Start_Position(LCD_VFPD);                               
  ER5517.LCD_VSYNC_Pulse_Width(LCD_VSPW);                              
      
  ER5517.Select_Main_Window_16bpp();

  ER5517.Memory_XY_Mode(); //Block mode (X-Y coordination addressing)
  ER5517.Memory_16bpp_Mode();
  ER5517.Select_Main_Window_16bpp();
}
void ER5517Basic::Display_ON(void)
{
/*  
Display ON/OFF
0b: Display Off.
1b: Display On.
*/
  unsigned char temp;
  
  ER5517.LCD_CmdWrite(0x12);
  temp = ER5517.LCD_DataRead();
  temp |= cSetb6;
  ER5517.LCD_DataWrite(temp);
}

  

ER5517Basic ER5517=ER5517Basic();

