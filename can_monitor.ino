
/* CAN Bus RX test
 *  
 * www.skpang.co.uk
 * 
 * V1.0 Dec 2016
 *  
 * For use with Teensy CAN-Bus demo board:
 * http://skpang.co.uk/catalog/teensy-canbus-demo-board-included-teensy-32-p-1505.html
 * 
 * Make sure the fonts are installed first
 * https://github.com/PaulStoffregen/ILI9341_fonts
 * 
 * Put the font files in /hardware/teensy/avr/libraries/ILI9341_t3 folder
 * 
 * Also requres new FlexCAN libarary
 * https://github.com/collin80/FlexCAN_Library
 * 
 * 
 */
#include "SPI.h"
#include "ILI9341_t3.h"
#include "font_Arial.h"
#include "font_LiberationMono.h"
#include "font_CourierNew.h"
#include <Metro.h>
#include <FlexCAN.h>
#include <Encoder.h>
//#include <SD.h>       /* Library from Adafruit.com */

#define SCK_PIN   13  //Clock pin
#define MISO_PIN  12  //Mater in Slave output
#define MOSI_PIN  11  //Master out Slave input
#define SD_PIN    10  //pin for SD card control
#define TFT_DC  9
#define TFT_CS 2

#define LASTVIEW_LIMIT  13
#define LOG_LIMIT 1024

#define NEW_MSG    (1<<0)
#define NEW_COUNT  (1<<1)

#define LASTVIEW_MODE  1
#define LOG_MODE       2
const int JOY_LEFT = 0;
const int JOY_RIGHT= 6;
const int JOY_CLICK = 5;

const int JOY_UP  = 7;
const int JOY_DOWN = 1;

const int JOG_A = 20;
const int JOG_B = 19;

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC);

Metro pauseLed = Metro(1000);// milliseconds
unsigned long time;  //used for time stamp

IntervalTimer CANTimer;

static CAN_message_t rxmsg;

typedef struct rx_msg_t{
  uint32_t count;
  uint32_t id; // can identifier
  uint8_t ext; // identifier is extended
  uint8_t len; // length of data
  uint8_t buf[8];
  uint8_t store_status;

}rx_msg_t;

typedef struct rx_log_t{
  uint32_t index;
  uint32_t id; // can identifier
  uint8_t ext; // identifier is extended
  uint8_t len; // length of data
  uint8_t buf[8];


}rx_log_t;


static rx_log_t rx_log[LOG_LIMIT];
static rx_msg_t rx_lastview[LASTVIEW_LIMIT];

volatile uint8_t system_status = 0;

int lastview_msg_count = 0;    //Number of message stored
uint32_t log_msg_count = 0;

uint8_t mode = 0;

char strbuf[100];
int led = 8;


// -------------------------------------------------------------
void setup(void)
{
  Can0.begin(500000); 
  pinMode(JOY_LEFT, INPUT_PULLUP);
  pinMode(JOY_RIGHT, INPUT_PULLUP);
  pinMode(JOY_CLICK, INPUT_PULLUP);
  pinMode(JOY_UP, INPUT_PULLUP);
  pinMode(JOY_DOWN, INPUT_PULLUP); 
  pinMode(led,OUTPUT);
  
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW);
  
 // tft.drawLine(21,0,21,240,ILI9341_LIGHTGREY);
  tft.drawLine(0,20,340,20,ILI9341_DARKGREY);    //Horizontal line
  tft.drawLine(28,20,28,240,ILI9341_DARKGREY);  // V line 
  
  tft.drawLine(260,20,260,240,ILI9341_DARKGREY);
  tft.drawLine(222,20,222,240,ILI9341_DARKGREY);

  tft.setFont(LiberationMono_10);
  tft.setCursor(0, 0);
  
  tft.println("CAN-Bus Monitor v1.0 skpang.co.uk 12/16");

  /* Clear array */
  for(int i=0;i<LASTVIEW_LIMIT;i++)
  {
      rx_lastview[i].store_status = 0;
      rx_lastview[i].id = 0;
      rx_lastview[i].len = 0;
      rx_lastview[i].count= 0;
  }

  memset(rx_lastview,0,sizeof(rx_lastview));
  memset(rx_log,0,sizeof(rx_log));
  
  delay(1000);
  Serial.println("CAN bus monitor");
 
  CANTimer.begin(checkCAN, 1000);    /* Start interrutp timer at 1mS */
 
  mode = LASTVIEW_MODE;
  tft.setCursor(0, 225);
  tft.println("Last View Mode");
}

// -------------------------------------------------------------
void loop()
{
  
    if(digitalRead(JOY_DOWN) == 0)
    {
        Serial.println("Clear store");
        lastview_msg_count = 0;
    
        for(int i=0;i<LASTVIEW_LIMIT; i++)
        {
                tft.fillRect(0,25+(i*14),27,10,ILI9341_BLACK); // Clear ID area
            
                tft.fillRect(34,25+(i*14),185,10,ILI9341_BLACK); // Clear data area
          
                tft.fillRect(262,25+(i*14),55,10,ILI9341_BLACK); // Clear count area
                rx_lastview[i].store_status = 0;
        }         
    }

    if(digitalRead(JOY_UP) == 0)
    {
        updatelcd();
        Serial.println("Update LCD");
        delay(50);
        while(digitalRead(JOY_UP) == 0);
    }

    if(digitalRead(JOY_RIGHT) == 0)
    {
        mode = LOG_MODE;
        
        updatelcd();
        Serial.println("  LCD");
        delay(50);
        tft.fillRect(0,225,150,15,ILI9341_BLACK); // Clear count area
      
        tft.setCursor(0, 225);
        tft.println("Log Mode");
      
        while(digitalRead(JOY_UP) == 0);
    }

    if(digitalRead(JOY_LEFT) == 0)
    {
        mode = LASTVIEW_MODE;
        
        updatelcd();
        
        delay(50);
        tft.fillRect(0,225,150,15,ILI9341_BLACK); // Clear count area
      
        tft.setCursor(0, 225);
        tft.println("Last View Mode");
      
        while(digitalRead(JOY_UP) == 0);
    }

    
     switch(mode)
     {
       case LASTVIEW_MODE:
         lastview_mode();
       
         break;
     
       case LOG_MODE:
         log_msg_mode();  
       
         break;
     
        default:         
         break;
     
     }

}//end loop 

void  log_msg_mode(void)
{

  //TO DO  



}


void lastview_mode(void)
{

     if(system_status ==1)
        {
            for(int i=0;i<lastview_msg_count;i++)
            {
                  if(rx_lastview[i].store_status & NEW_MSG)
                  {
                      updatelcd_newline(i);
                      rx_lastview[i].store_status &= (~NEW_MSG);
                  }
                  
                  if(rx_lastview[i].store_status & NEW_COUNT)
                  {
                      updatelcd_count(i);
                      rx_lastview[i].store_status &=(~NEW_COUNT) ;
                  }
            }
          system_status = 0; 
        }

}
 
/* From Timer Interrupt */
void checkCAN(void)
{

      //if (CANbus.available()) {   //is the CAN available?      assume it is or we wouldn't have got this far.
       if (Can0.read(rxmsg))
       {   
       
           digitalWrite(led,HIGH);

           switch(mode)
           {
             case LASTVIEW_MODE:
               store_lastview();
             
               break;
           
             case LOG_MODE:
               log_msg();  
             
               break;
           
              default:         
               break;
           
           }
           digitalWrite(led,LOW);
       }
}

void log_msg(void)
{

            for(uint8_t lp=0;lp<rxmsg.len;lp++)    // Store new message
             {
               rx_log[log_msg_count].buf[lp] = rxmsg.buf[lp];
             }
             rx_log[log_msg_count].id = 
             rx_log[log_msg_count].len = rxmsg.len;
             rx_log[log_msg_count].index= 1;

             if(log_msg_count <LOG_LIMIT)
                  log_msg_count++;
                  
             system_status = 1;     
}


void store_lastview(void)
{

    uint8_t chkmsg;

    if(lastview_msg_count == 0)
      {  
           /* Very first message, just store it */
           for(int lp=0;lp<rxmsg.len;lp++)    // Store new message
             {
               rx_lastview[lastview_msg_count].buf[lp] = rxmsg.buf[lp];
             }
             rx_lastview[lastview_msg_count].id = rxmsg.id;
             rx_lastview[lastview_msg_count].len = rxmsg.len;
             rx_lastview[lastview_msg_count].count= 1;

             rx_lastview[lastview_msg_count].store_status |= NEW_MSG;  /* Set a flag so it will get pickup in the main loop */
             lastview_msg_count++;
             
       }else
       {   /* Not first message, check everyone in rx_lastview*/
           for(chkmsg = 0;chkmsg<lastview_msg_count;chkmsg++)
           {
                if((rxmsg.id == rx_lastview[chkmsg].id) && (rxmsg.len == rx_lastview[chkmsg].len))
                 {
                      /* Found the same message,increament count and exit for loop */
                      rx_lastview[chkmsg].count++;
                      rx_lastview[chkmsg].store_status |=NEW_COUNT;
                      break;                     
                  }
            }// for

            if(chkmsg == lastview_msg_count) /*We are at the end of rx_lastview check and no new message found */
            {
                /* Store new message */ 
                for(int lp=0;lp<rxmsg.len;lp++)
                {
                    rx_lastview[lastview_msg_count].buf[lp] = rxmsg.buf[lp];
                }
                rx_lastview[lastview_msg_count].id = rxmsg.id;
                rx_lastview[lastview_msg_count].len = rxmsg.len;
                rx_lastview[lastview_msg_count].count= 1;
                rx_lastview[lastview_msg_count].store_status |= NEW_MSG;
                if(lastview_msg_count <LASTVIEW_LIMIT)
                  lastview_msg_count++;
            }
       } 
 
       system_status = 1;

}


void updatelcd_newline(int store_index)
{
 
      String LCDStr("");

      for(int lp=0;lp<rx_lastview[store_index].len;lp++)
      {
          LCDStr += String(rx_lastview[store_index].buf[lp],HEX); 
          LCDStr += (" ");
      }
  
      tft.fillRect(0,25+(store_index*14),27,10,ILI9341_BLACK); // Clear ID area
      tft.setCursor(0, 25+(store_index*14));
      tft.print(rx_lastview[store_index].id,HEX);          // Display ID
  
      tft.fillRect(34,25+(store_index*14),185,10,ILI9341_BLACK); // Clear data area
      tft.setCursor(34, 25+(store_index*14));                    
      tft.print(LCDStr);                        // Display data 
  
      tft.fillRect(262,25+(store_index*14),55,10,ILI9341_BLACK); // Clear count area
      tft.setCursor(262, 25+(store_index*14));                    
      tft.print(rx_lastview[store_index].count,DEC);                        // Display data 
   

}
void updatelcd_count(int store_index)
{

     tft.fillRect(262,25+(store_index*14),55,10,ILI9341_BLACK); // Clear count area
     tft.setCursor(262, 25+(store_index*14));                    
     tft.print(rx_lastview[store_index].count,DEC);                        // Display data 
 

}

void updatelcd(void)
{
    
      String LCDStr("");
     
      for(int i=0;i<lastview_msg_count;i++)
      {
            LCDStr = "";
            for(int lp=0;lp<rx_lastview[i].len;lp++)
            {
                LCDStr += String(rx_lastview[i].buf[lp],HEX); 
                LCDStr += (" ");
            }
          
            tft.fillRect(0,25+(i*14),27,10,ILI9341_BLACK); // Clear ID area
            tft.setCursor(0, 25+(i*14));
            tft.print(rx_lastview[i].id,HEX);          // Display ID
          
            tft.fillRect(34,25+(i*14),180,10,ILI9341_BLACK); // Clear data area
            tft.setCursor(34, 25+(i*14));                    
            tft.print(LCDStr);                        // Display data 
          
            tft.fillRect(262,25+(i*14),55,10,ILI9341_BLACK); // Clear count area
            tft.setCursor(262, 25+(i*14));                    
            tft.print(rx_lastview[i].count,DEC);                        // Display data 
           
      }

}




