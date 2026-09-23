#ifndef LCD_init_h
#define LCD_init_h
#include "Arduino.h"
//#include <avr/pgmspace.h>
#include <SPI.h>

#define  SPI_CS     25
#define  SPI_RST    33
#define  SPI_DI     32
#define  SPI_CLK    13


#define  SPI_RST_0   digitalWrite(SPI_RST, LOW)
#define  SPI_RST_1   digitalWrite(SPI_RST, HIGH)
#define  SPI_CS_0  digitalWrite(SPI_CS, LOW)
#define  SPI_CS_1  digitalWrite(SPI_CS, HIGH)
#define  SPI_CLK_0   digitalWrite(SPI_CLK, LOW)
#define  SPI_CLK_1   digitalWrite(SPI_CLK, HIGH)
#define  SPI_DI_0   digitalWrite(SPI_DI, LOW)
#define  SPI_DI_1   digitalWrite(SPI_DI, HIGH)

#define hsize 720
#define vsize 1280
#define LINE 4


void SPI_WriteCmd(unsigned char Sdata) { 
	unsigned char i;	
	SPI_CS_0;
	SPI_DI_0;
	SPI_CLK_0; 
	SPI_CLK_1;
	
	for(i=8; i>0; i--) {
		if(Sdata&0x80)
			SPI_DI_1;
		else
			SPI_DI_0;
		SPI_CLK_0; 
		SPI_CLK_1;
		Sdata <<= 1;
	}
	SPI_CLK_0;
	SPI_CS_1;
}

void SPI_WriteData(unsigned char Sdata) {
	unsigned char i;
	SPI_CS_0;
	SPI_DI_1;
	SPI_CLK_0; 
	SPI_CLK_1;
	
	for(i=8; i>0; i--) {
		if(Sdata&0x80)
			SPI_DI_1;
		else
			SPI_DI_0;
		SPI_CLK_0; 
		SPI_CLK_1;
		Sdata <<= 1;
	}
	SPI_CLK_0;
	SPI_CS_1;
}


void GP_COMMAD_PA(unsigned int num) 
{
	SPI_WriteCmd(0xbc);
	SPI_WriteData(num&0xff);	
	SPI_WriteData((num>>8)&0xff);	
	SPI_WriteCmd(0xbf);
}




void  LCD_initial()
{

   GP_COMMAD_PA(4);  SPI_WriteData(0xFF);   SPI_WriteData(0x98);  SPI_WriteData(0x81);  SPI_WriteData(0x03);
     GP_COMMAD_PA(2);  SPI_WriteData(0x01);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x02);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x03);  SPI_WriteData(0x73);
     GP_COMMAD_PA(2);  SPI_WriteData(0x04);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x05);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x06);  SPI_WriteData(0x08);
     GP_COMMAD_PA(2);  SPI_WriteData(0x07);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x08);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x09);  SPI_WriteData(0x1B);
     GP_COMMAD_PA(2);  SPI_WriteData(0x0a);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x0b);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x0c);  SPI_WriteData(0x0D);
     GP_COMMAD_PA(2);  SPI_WriteData(0x0d);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x0e);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x0f);  SPI_WriteData(0x26);
     GP_COMMAD_PA(2);  SPI_WriteData(0x10);  SPI_WriteData(0x26); 
     GP_COMMAD_PA(2);  SPI_WriteData(0x11);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x12);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x13);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x14);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x15);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x16);  SPI_WriteData(0x00); 
     GP_COMMAD_PA(2);  SPI_WriteData(0x17);  SPI_WriteData(0x00); 
     GP_COMMAD_PA(2);  SPI_WriteData(0x18);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x19);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x1a);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x1b);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x1c);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x1d);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x1e);  SPI_WriteData(0x40);
     GP_COMMAD_PA(2);  SPI_WriteData(0x1f);  SPI_WriteData(0x00);  
     GP_COMMAD_PA(2);  SPI_WriteData(0x20);  SPI_WriteData(0x06);
     GP_COMMAD_PA(2);  SPI_WriteData(0x21);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x22);  SPI_WriteData(0x00);   
     GP_COMMAD_PA(2);  SPI_WriteData(0x23);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x24);  SPI_WriteData(0x00);  
     GP_COMMAD_PA(2);  SPI_WriteData(0x25);  SPI_WriteData(0x00); 
     GP_COMMAD_PA(2);  SPI_WriteData(0x26);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x27);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x28);  SPI_WriteData(0x33);  
     GP_COMMAD_PA(2);  SPI_WriteData(0x29);  SPI_WriteData(0x03);
     GP_COMMAD_PA(2);  SPI_WriteData(0x2a);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x2b);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x2c);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x2d);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x2e);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x2f);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x30);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x31);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x32);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x33);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x34);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x35);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x36);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x37);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x38);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x39);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x3a);  SPI_WriteData(0x00); 
     GP_COMMAD_PA(2);  SPI_WriteData(0x3b);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x3c);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x3d);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x3e);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x3f);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x40);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x41);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x42);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x43);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x44);  SPI_WriteData(0x00);


//GIP_2
     GP_COMMAD_PA(2);  SPI_WriteData(0x50);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x51);  SPI_WriteData(0x23);
     GP_COMMAD_PA(2);  SPI_WriteData(0x52);  SPI_WriteData(0x45);
     GP_COMMAD_PA(2);  SPI_WriteData(0x53);  SPI_WriteData(0x67);
     GP_COMMAD_PA(2);  SPI_WriteData(0x54);  SPI_WriteData(0x89);
     GP_COMMAD_PA(2);  SPI_WriteData(0x55);  SPI_WriteData(0xab);
     GP_COMMAD_PA(2);  SPI_WriteData(0x56);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x57);  SPI_WriteData(0x23);
     GP_COMMAD_PA(2);  SPI_WriteData(0x58);  SPI_WriteData(0x45);
     GP_COMMAD_PA(2);  SPI_WriteData(0x59);  SPI_WriteData(0x67);
     GP_COMMAD_PA(2);  SPI_WriteData(0x5a);  SPI_WriteData(0x89);
     GP_COMMAD_PA(2);  SPI_WriteData(0x5b);  SPI_WriteData(0xab);
     GP_COMMAD_PA(2);  SPI_WriteData(0x5c);  SPI_WriteData(0xcd);
     GP_COMMAD_PA(2);  SPI_WriteData(0x5d);  SPI_WriteData(0xef);

//GIP_3
     GP_COMMAD_PA(2);  SPI_WriteData(0x5e);  SPI_WriteData(0x11);
     GP_COMMAD_PA(2);  SPI_WriteData(0x5f);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x60);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x61);  SPI_WriteData(0x07);
     GP_COMMAD_PA(2);  SPI_WriteData(0x62);  SPI_WriteData(0x06);
     GP_COMMAD_PA(2);  SPI_WriteData(0x63);  SPI_WriteData(0x0E);
     GP_COMMAD_PA(2);  SPI_WriteData(0x64);  SPI_WriteData(0x0F);
     GP_COMMAD_PA(2);  SPI_WriteData(0x65);  SPI_WriteData(0x0C);
     GP_COMMAD_PA(2);  SPI_WriteData(0x66);  SPI_WriteData(0x0D);
     GP_COMMAD_PA(2);  SPI_WriteData(0x67);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x68);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x69);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6a);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6b);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6c);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6d);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6e);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6f);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x70);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x71);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x72);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x73);  SPI_WriteData(0x05);
     GP_COMMAD_PA(2);  SPI_WriteData(0x74);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x75);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x76);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x77);  SPI_WriteData(0x07);
     GP_COMMAD_PA(2);  SPI_WriteData(0x78);  SPI_WriteData(0x06);
     GP_COMMAD_PA(2);  SPI_WriteData(0x79);  SPI_WriteData(0x0E);
     GP_COMMAD_PA(2);  SPI_WriteData(0x7a);  SPI_WriteData(0x0F);
     GP_COMMAD_PA(2);  SPI_WriteData(0x7b);  SPI_WriteData(0x0C);
     GP_COMMAD_PA(2);  SPI_WriteData(0x7c);  SPI_WriteData(0x0D);
     GP_COMMAD_PA(2);  SPI_WriteData(0x7d);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x7e);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x7f);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x80);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x81);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x82);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x83);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x84);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x85);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x86);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x87);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x88);  SPI_WriteData(0x02);
     GP_COMMAD_PA(2);  SPI_WriteData(0x89);  SPI_WriteData(0x05);
     GP_COMMAD_PA(2);  SPI_WriteData(0x8A);  SPI_WriteData(0x01);


//CMD_Page 4
     GP_COMMAD_PA(4);  SPI_WriteData(0xFF);  SPI_WriteData(0x98);  SPI_WriteData(0x81);  SPI_WriteData(0x04);
//     GP_COMMAD_PA(2);  SPI_WriteData(0x00);  SPI_WriteData(0x80);
     GP_COMMAD_PA(2);  SPI_WriteData(0x38);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x39);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6C);  SPI_WriteData(0x15);
     GP_COMMAD_PA(2);  SPI_WriteData(0x6E);  SPI_WriteData(0x1A);               //di_pwr_reg=0 VGH clamp 15V
     GP_COMMAD_PA(2);  SPI_WriteData(0x6F);  SPI_WriteData(0x25);              // reg vcl + VGH pumping ratio 3x VGL=-2x
     GP_COMMAD_PA(2);  SPI_WriteData(0x3A);  SPI_WriteData(0xA4);               
     GP_COMMAD_PA(2);  SPI_WriteData(0x8D);  SPI_WriteData(0x20);               //VGL clamp -10V
     GP_COMMAD_PA(2);  SPI_WriteData(0x87);  SPI_WriteData(0xBA);  
     GP_COMMAD_PA(2);  SPI_WriteData(0x3B);  SPI_WriteData(0x98);             

//CMD_Page 1
     GP_COMMAD_PA(4);  SPI_WriteData(0xFF);  SPI_WriteData(0x98);  SPI_WriteData(0x81);   SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x22);  SPI_WriteData(0x0A);		//BGR);  SPI_WriteData(0x SS
     GP_COMMAD_PA(2);  SPI_WriteData(0x31);  SPI_WriteData(0x00);		//column inversion
     GP_COMMAD_PA(2);  SPI_WriteData(0x50);  SPI_WriteData(0x6B);         	//VREG1OUT=3.996V
     GP_COMMAD_PA(2);  SPI_WriteData(0x51);  SPI_WriteData(0x66);         	//VREG2OUT=-3.996V
     GP_COMMAD_PA(2);  SPI_WriteData(0x53);  SPI_WriteData(0x73);		//VCOM1
     GP_COMMAD_PA(2);  SPI_WriteData(0x55);  SPI_WriteData(0x8B);		//VCOM2
     GP_COMMAD_PA(2);  SPI_WriteData(0x60);  SPI_WriteData(0x1B);               //SDT
     GP_COMMAD_PA(2);  SPI_WriteData(0x61);  SPI_WriteData(0x01);
     GP_COMMAD_PA(2);  SPI_WriteData(0x62);  SPI_WriteData(0x0C);
     GP_COMMAD_PA(2);  SPI_WriteData(0x63);  SPI_WriteData(0x00);

//VP255	Gamma P
     GP_COMMAD_PA(2);  SPI_WriteData(0xA0);  SPI_WriteData(0x00);
//VP251		
     GP_COMMAD_PA(2);  SPI_WriteData(0xA1);  SPI_WriteData(0x15);
//VP247                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xA2);  SPI_WriteData(0x1F);
//VP243                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xA3);  SPI_WriteData(0x13);
//VP239                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xA4);  SPI_WriteData(0x11);
//VP231                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xA5);  SPI_WriteData(0x21);
//VP219                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xA6);  SPI_WriteData(0x17);
//VP203                      
     GP_COMMAD_PA(2);  SPI_WriteData(0xA7);  SPI_WriteData(0x1B);
//VP175                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xA8);  SPI_WriteData(0x6B);
//VP144                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xA9);  SPI_WriteData(0x1E);
//VP111                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xAA);  SPI_WriteData(0x2B);
//VP80                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xAB);  SPI_WriteData(0x5D);
//VP52                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xAC);  SPI_WriteData(0x19);
//VP36                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xAD);  SPI_WriteData(0x14);
//VP24                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xAE);  SPI_WriteData(0x4B);
//VP16                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xAF);  SPI_WriteData(0x1D);
//VP12                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xB0);  SPI_WriteData(0x27);
//VP8                      
     GP_COMMAD_PA(2);  SPI_WriteData(0xB1);  SPI_WriteData(0x49);
//VP4                         
     GP_COMMAD_PA(2);  SPI_WriteData(0xB2);  SPI_WriteData(0x5D);
//VP0                          
     GP_COMMAD_PA(2);  SPI_WriteData(0xB3);  SPI_WriteData(0x39);                        


//VN255 GAMMA N                                               
     GP_COMMAD_PA(2);  SPI_WriteData(0xC0);  SPI_WriteData(0x00);
//VP251		
     GP_COMMAD_PA(2);  SPI_WriteData(0xC1);  SPI_WriteData(0x01);
//VP247                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xC2);  SPI_WriteData(0x0C);
//VP243                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xC3);  SPI_WriteData(0x11);
//VP239                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xC4);  SPI_WriteData(0x15);
//VP231                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xC5);  SPI_WriteData(0x28);
//VP219                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xC6);  SPI_WriteData(0x1B);
//VP203                      
     GP_COMMAD_PA(2);  SPI_WriteData(0xC7);  SPI_WriteData(0x1C);
//VP175                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xC8);  SPI_WriteData(0x62);
//VP144                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xC9);  SPI_WriteData(0x1C);
//VP111                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xCA);  SPI_WriteData(0x29);
//VP80                       
     GP_COMMAD_PA(2);  SPI_WriteData(0xCB);  SPI_WriteData(0x60);
//VP52                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xCC);  SPI_WriteData(0x16);
//VP36                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xCD);  SPI_WriteData(0x17);
//VP24                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xCE);  SPI_WriteData(0x4A);
//VP16                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xCF);  SPI_WriteData(0x23);
//VP12                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xD0);  SPI_WriteData(0x24);
//VP8                        
     GP_COMMAD_PA(2);  SPI_WriteData(0xD1);  SPI_WriteData(0x4F);
//VP4                         
     GP_COMMAD_PA(2);  SPI_WriteData(0xD2);  SPI_WriteData(0x5F);
//VP0                          
     GP_COMMAD_PA(2);  SPI_WriteData(0xD3);  SPI_WriteData(0x39);                   



//CMD_Page 0
     GP_COMMAD_PA(4);  SPI_WriteData(0xFF);  SPI_WriteData(0x98);  SPI_WriteData(0x81);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x35);  SPI_WriteData(0x00);
     GP_COMMAD_PA(2);  SPI_WriteData(0x11);  SPI_WriteData(0x00);
     delay(0x120);
     GP_COMMAD_PA(2);  SPI_WriteData(0x29);  SPI_WriteData(0x00);
}


void SSD2828_Initial(void)
{  
    pinMode(SPI_CS,   OUTPUT);   
    pinMode(SPI_CLK,   OUTPUT);
    pinMode(SPI_DI,   OUTPUT);
    pinMode(SPI_RST,   OUTPUT); 
    SPI_RST_1;
    SPI_CS_1;
    SPI_CLK_0;
    SPI_DI_1;
    SPI_RST_0;						   
    delay(30);
    SPI_RST_1;
    delay(50);	 
   
	SPI_WriteCmd(0xb7);
	SPI_WriteData(0x50);//50=TX_CLK 70=PCLK
	SPI_WriteData(0x00);   //Configuration Register

	SPI_WriteCmd(0xb8);
	SPI_WriteData(0x00);
	SPI_WriteData(0x00);   //VC(Virtual ChannelID) Control Register

	SPI_WriteCmd(0xb9);
	SPI_WriteData(0x00);//1=PLL disable
	SPI_WriteData(0x00);



 	SPI_WriteCmd(0xBA);//PLL=(TX_CLK/MS)*NS 
	SPI_WriteData(0x14);//14); SPI_WriteData(D7-0=NS(0x01 : NS=1)
	SPI_WriteData(0x42);//42); SPI_WriteData(D15-14=PLL00=62.5-125 01=126-250 10=251-500 11=501-1000  DB12-8=MS(01:MS=1)



	SPI_WriteCmd(0xBB);//LP Clock Divider LP clock = 400MHz / LPD / 8 = 480 / 8/ 8 = 7MHz
	SPI_WriteData(0x03);//D5-0=LPD=0x1 �?Divide by 2
	SPI_WriteData(0x00);

	 SPI_WriteCmd(0xb9);
	SPI_WriteData(0x01);//1=PLL disable
	SPI_WriteData(0x00);
	//MIPI lane configuration
	SPI_WriteCmd(0xDE);
	SPI_WriteData(0x00);//11=4LANE 10=3LANE 01=2LANE 00=1LANE
	SPI_WriteData(0x00);

	SPI_WriteCmd(0xc9);
	SPI_WriteData(0x02);
	SPI_WriteData(0x23);   //p1: HS-Data-zero  p2: HS-Data- prepare  --> 8031 issue
//	delay(100);


         LCD_initial();	  

				 

	SPI_WriteCmd(0xb7);
	SPI_WriteData(0x50);
	SPI_WriteData(0x00);   //Configuration Register

	SPI_WriteCmd(0xb8);
	SPI_WriteData(0x00);
	SPI_WriteData(0x00);   //VC(Virtual ChannelID) Control Register

	SPI_WriteCmd(0xb9);
	SPI_WriteData(0x00);//1=PLL disable
	SPI_WriteData(0x00);

        SPI_WriteCmd(0xBA);//
	SPI_WriteData(0x2D);//14); SPI_WriteData(D7-0=NS(0x01 : NS=1)		 //0x25
	SPI_WriteData(0x82);//42); SPI_WriteData(D15-14=PLL00=62.5-125 01=126-250 10=251-500 11=501-1000  DB12-8=MS(01:MS=1)


	SPI_WriteCmd(0xBB);//LP Clock Divider LP clock = 400MHz / LPD / 8 = 480 / 8/ 8 = 7MHz
	SPI_WriteData(0x07);//D5-0=LPD=0x1 �?Divide by 2
	SPI_WriteData(0x00);

	SPI_WriteCmd(0xb9);
	SPI_WriteData(0x01);//1=PLL disable
	SPI_WriteData(0x00);

	SPI_WriteCmd(0xc9);
	SPI_WriteData(0x02);
	SPI_WriteData(0x23);   //p1: HS-Data-zero  p2: HS-Data- prepare  --> 8031 issue
	delay(100);

	SPI_WriteCmd(0xCA);
	SPI_WriteData(0x01);//CLK Prepare
	SPI_WriteData(0x23);//Clk Zero

	SPI_WriteCmd(0xCB); //local_write_reg(addr=0xCB); SPI_WriteData(data=0x0510)
	SPI_WriteData(0x10); //Clk Post
	SPI_WriteData(0x05); //Clk Per

	SPI_WriteCmd(0xCC); //local_write_reg(addr=0xCC); SPI_WriteData(data=0x100A)
	SPI_WriteData(0x05); //HS Trail
	SPI_WriteData(0x10); //Clk Trail

	SPI_WriteCmd(0xD0);
	SPI_WriteData(0x00);
	SPI_WriteData(0x00);  



	
        SPI_WriteCmd(0xB1); //local_write_reg(addr=0xB2); SPI_WriteData(data=0x1224)
	SPI_WriteData(LCD_HSPW); //HSA
	SPI_WriteData(LCD_VSPW); //VSA 


	SPI_WriteCmd(0xB2); //local_write_reg(addr=0xB2); SPI_WriteData(data=0x1224)
	SPI_WriteData(LCD_HBPD); //HBP
	SPI_WriteData(LCD_VBPD); //VBP 

	SPI_WriteCmd(0xB3); //local_write_reg(addr=0xB3); SPI_WriteData(data=0x060C)
	SPI_WriteData(LCD_HFPD); //HFP
	SPI_WriteData(LCD_VFPD); //VFP  

 
	SPI_WriteCmd(0xB4);//Horizontal active period 
	SPI_WriteData(hsize);//
	SPI_WriteData(hsize>>8);

	SPI_WriteCmd(0xB5);//Vertical active period 
	SPI_WriteData(vsize);//
	SPI_WriteData(vsize>>8);

	
	SPI_WriteCmd(0xB6);//RGB CLK  16BPP=00 18BPP=01
	SPI_WriteData(0x0b);//D7=0 D6=0 D5=0  D1-0=11 �?24bpp	 //07
	SPI_WriteData(0x00);//D15=VS D14=HS D13=CLK D12-9=NC D8=0=Video with blanking packet. 00-F0
	 


	//MIPI lane configuration
	SPI_WriteCmd(0xDE);//
	SPI_WriteData(0x03);//11=4LANE 10=3LANE 01=2LANE 00=1LANE
	SPI_WriteData(0x00);

	SPI_WriteCmd(0xD6);//  05=BGR  04=RGB
	SPI_WriteData(0x01);//D0=0=RGB 1:BGR D1=1=Most significant byte sent first
	SPI_WriteData(0x00);

 	SPI_WriteCmd(0xDB);
	SPI_WriteData(0x58);
	SPI_WriteData(0x00);


	SPI_WriteCmd(0xB7);
	SPI_WriteData(0x4B);   
	SPI_WriteData(0x02);

	SPI_WriteCmd(0x2c);		     

}

#endif


