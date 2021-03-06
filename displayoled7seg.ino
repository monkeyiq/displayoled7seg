#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <MultiLCD.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "LedControl.h"

/*
 Now we need a LedControl to work with.
 ***** These pin numbers will probably not work with your hardware *****
 pin 5 is connected to the DataIn 
 pin 6 is connected to the CLK 
 pin 2 is connected to LOAD 
 We have only a single MAX72XX.
 */
LedControl lc=LedControl(5,6,7,2);

RF24 radio(9,10);
// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D3LL };

enum { RMSG_JOYXY = 1, MRSG_SIMPLEPROX = 2, RMSG_INFO = 3  } RADIOMSGTYPES;
struct radiomsg {
  int type;
  union {
    struct {
      int h;
      int v;
    } joyxy;
    struct {
      int distancecm;
      int angle;
    } prox;    
    struct {
      char buf[20];
    } info;
    int dummy;
  };
};



LCD_SSD1306 lcd;




static const PROGMEM uint8_t smile[48 * 48 / 8] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xC0,0xE0,0xF0,0xF8,0xF8,0xFC,0xFC,0xFE,0xFE,0x7E,0x7F,0x7F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x7F,0x7F,0x7E,0xFE,0xFE,0xFC,0xFC,0xF8,0xF8,0xF0,0xE0,0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xC0,0xF0,0xFC,0xFE,0xFF,0xFF,0xFF,0x3F,0x1F,0x0F,0x07,0x03,0x01,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x01,0x03,0x07,0x0F,0x1F,0x3F,0xFF,0xFF,0xFF,0xFE,0xFC,0xF0,0xC0,0x00,
0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x1F,0x1F,0x1F,0x3F,0x1F,0x1F,0x02,0x00,0x00,0x00,0x00,0x06,0x1F,0x1F,0x1F,0x3F,0x1F,0x1F,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0xE0,0x00,0x00,0x30,0xF8,0xF8,0xF8,0xF8,0xE0,0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xC0,0xE0,0xF8,0xF8,0xFC,0xF8,0x30,0x00,0x00,0xE0,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,
0x00,0x03,0x0F,0x3F,0x7F,0xFF,0xFF,0xFF,0xFC,0xF8,0xF0,0xE1,0xC7,0x87,0x0F,0x1F,0x3F,0x3F,0x3E,0x7E,0x7C,0x7C,0x7C,0x78,0x78,0x7C,0x7C,0x7C,0x7E,0x3E,0x3F,0x3F,0x1F,0x0F,0x87,0xC7,0xE1,0xF0,0xF8,0xFC,0xFF,0xFF,0xFF,0x7F,0x3F,0x0F,0x03,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x03,0x07,0x0F,0x1F,0x1F,0x3F,0x3F,0x7F,0x7F,0x7E,0xFE,0xFE,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFE,0xFE,0x7E,0x7F,0x7F,0x3F,0x3F,0x1F,0x1F,0x0F,0x07,0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
};


// display number, value
void printNumber(byte d, int v) {
    int ones;
    int tens;
    int hundreds;
    boolean negative;	

    if(v < -9999 || v > 9999) 
       return;
    if(v<0) {
        negative=true;
        v=v*-1;
    }
    ones=v%10;
    v=v/10;
    tens=v%10;
    v=v/10;
    hundreds=v%10;			
    v=v/10;
    int grands=v%10;	
/*    if(negative) {
       //print character '-' in the leftmost column	
       lc.setChar(0,3,'-',false);
    }
    else {
       //print a blank in the sign column
       lc.setChar(0,3,' ',false);
    }
    */
    //Now print the number digit by digit
    if( grands != 0 )
       lc.setDigit(d,0,(byte)grands,false);
    else
       lc.setChar(d,0,' ',false );
    lc.setDigit(d,1,(byte)hundreds,false);
    lc.setDigit(d,2,(byte)tens,false);
    lc.setDigit(d,3,(byte)ones,false);
}

int joyv = 512;
int joyh = 512;
int distancecm = 0, angle = 0;
char info[40];

void setup()
{
  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  lc.shutdown(0,false);
  lc.shutdown(1,false);
  
  /* Set the brightness to a medium values */
  lc.setIntensity(0,8);
  /* and clear the display */
  lc.clearDisplay(0);

  lc.setIntensity(1,8);
  lc.clearDisplay(1);
  
      lc.setDigit(0,0, 7,false);
      lc.setDigit(0,1, 7,false);
      lc.setDigit(0,2, 3,false);
      lc.setDigit(0,3, 1,false);

int i=0;
int loops = 2;
   for(; loops > 0; loops--) {
   for( i = 0; i < 10; i++ ) {
     for(int j=0; j<2; j++ ) {
    //   lc.clearDisplay(0);

      lc.setDigit(j,0, i,false);
      lc.setDigit(j,1, i,false);
      lc.setDigit(j,2, i,false);
      lc.setDigit(j,3, i,false);
   }
      delay(100);
   }
   }
    
  
  
      Serial.begin(57600);

   	lcd.begin();
        lcd.draw(smile, 40, 8, 48, 48);
	delay(1000);

    radio.begin();
    radio.setRetries(15,15);
       
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
      
    radio.startListening();
    radio.printDetails();
  
lcd.clear();
     strcpy(info,"");
}


void loop()
{
//	lcd.clear();
//	lcd.draw(smile, 40, 8, 48, 48);
//	delay(1000);



    // if there is data ready
    if ( radio.available() )
    {
//        Serial << "packet!!!!..." << endl;
  
        // Dump the payloads until we've gotten everything
        unsigned long got_time;
        bool done = false;
        while (!done)
        {
            struct radiomsg msg;
            msg.type = 0;
            // Fetch the payload, and see if this was the last one.
            done = radio.read( &msg, sizeof(radiomsg) );

//            printf("Got payload done:%d %d  h:%d v:%d...\n\r", done, msg.type, msg.joyxy.h, msg.joyxy.v );
            if( done ) 
            {
              switch( msg.type ) 
              {
                 case MRSG_SIMPLEPROX:
                     distancecm = msg.prox.distancecm;
                     angle = msg.prox.angle;
                    break;       
                 case RMSG_JOYXY:              
                    joyh = msg.joyxy.h;
                    joyv = msg.joyxy.v;  
                    break;   
                 case RMSG_INFO:
                    strcpy(info,msg.info.buf);
                    break;   

            } 

            }
        }
    }

printNumber(0, joyh);
printNumber(1, joyv);


	//lcd.clear();

        char buf[100];
        sprintf(buf,"h:%4d v:%4d", joyh, joyv);
        lcd.setCursor(0, 0);
	lcd.setFont(FONT_SIZE_SMALL);
	lcd.print(buf);

        sprintf(buf,"angle:%4d cm:%4d", angle, distancecm);
        lcd.setCursor(0, 1);
	lcd.setFont(FONT_SIZE_SMALL);
	lcd.print(buf);

        sprintf(buf,"%s", info);
        lcd.setCursor(0, 2);
	lcd.setFont(FONT_SIZE_SMALL);
	lcd.print(buf);


	lcd.setCursor(0, 5);
	lcd.setFont(FONT_SIZE_LARGE);
	lcd.printLong(12345671);

            // Delay just a little bit to let the other unit
            // make the transition to receiver
            delay(20);
//	delay(1000);
}
