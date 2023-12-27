/*****************************************************
 * FlipDot 37c3 
 *
 * (c) 2023 Dario Carluccio
 * 
 * CPU-Board: Arduino Nano
 * 
 * Arduino Settings:
 *    Board:  Arduino Nano
 *    Processor: ATmega 328P
 *
 * Connections:
 * to CD4514 4:16 Multiplexer
 * - D6         //  A0 from CD4514     Multiplexer controls the ROW Drivers
 * - D7         //  A1 from CD4514      - Low-Side Driver [row]  = {13, 12, 15, 14, 9, 8, 11, 10}
 * - D5         //  A2 from CD4514      - High-Side Driver [row] = {0, 2, 1, 3, 4, 5, 6, 7}
 * - D4         //  A3 from CD4514 
 * - D3         // /EN from LOWER CD4514 (Row 0-7)
 * - D2         // /EN from UPPER CD4514 (Row 8-15) 
 * to Display Board Connector J2 
 * - A4         // J2-16 to FP2800 Address Bit 0 (Segmanet A0)
 * - A3         // J2-15 to FP2800 Address Bit 1 (Segmanet A1)
 * - A2         // J2-14 to FP2800 Address Bit 2 (Segmanet A2)
 * - A0         // J2-12 to FP2800 Address Bit 3 (Digit B0)
 * - D13        // J2-11 to FP2800 Address Bit 4 (Digit B1)
 * - A1         // J2-13 to FP2800 Data
 * - D11        // J2-10 to FP2800 Enable of 1st Panel  J1-7
 * - D8         // J2-9  to FP2800 Enable of 2nd Panel  J1-10
 * - D9         // J2-8  to FP2800 Enable of 3rd Panel  J1-9
 * - D10        // J2-7  to FP2800 Enable of 4th Panel  J1-8
 * - D12        // to FP2800 Enable of 5th Panel (seperate cable)
 * LED Driver
 * - A5         // drive LED (not used because ugly)  
 ******************************************************/
#include <Arduino.h>
#include <avr/wdt.h>

// Definition of Matrix Module
#include "include\LAWO_Matrix_Side.h"    // Side-Ddisplay 112*16  
// #include "include\LAWO_Matrix_Front.h    // Front-Display 140*16  
// #include "include\LAWO_Matrix_Panel16.h" // Backdisplay 28*16

// passwordhints
#include "pass\passwordhints.h"

// font struct
#include "include\gfxfont.h"

// fonts
#include "font\FlipDot16.h"
#include "font\FlipDot8.h"
#include "font\FlipDot14.h"

/*********************************
 * GPIO Settings
 *********************************/
// to CD4514 4:16 Multiplexer
#define ROW_A0 6         //  A0 from CD4514 
#define ROW_A1 7         //  A1 from CD4514 
#define ROW_A2 5         //  A2 from CD4514 
#define ROW_A3 4         //  A3 from CD4514 
#define ROWS_BOTTOM 3    // /EN from LOWER CD4514 (Row 0-7)
#define ROWS_TOP 2       // /EN from UPPER CD4514 (Row 8-15) 
// to Display Board Connector J2 
#define COL_A0 A4        // J2-16 to FP2800 Address Bit 0 (Segmanet A0)
#define COL_A1 A3        // J2-15 to FP2800 Address Bit 1 (Segmanet A1)
#define COL_A2 A2        // J2-14 to FP2800 Address Bit 2 (Segmanet A2)
#define COL_A3 A0        // J2-12 to FP2800 Address Bit 3 (Digit B0)
#define COL_A4 13        // J2-11 to FP2800 Address Bit 4 (Digit B1)
#define D A1             // J2-13 to FP2800 Data
#define E1 11            // J2-10 to FP2800 Enable of 1st Panel  J1-7
#define E2 8             // J2-9  to FP2800 Enable of 2nd Panel  J1-10
#define E3 9             // J2-8  to FP2800 Enable of 3rd Panel  J1-9
#define E4 10            // J2-7  to FP2800 Enable of 4th Panel  J1-8
#define E5 12            // to FP2800 Enable of 5th Panel (seperate cable)
// LED Driver
#define LED A5

/*********************************
 * DEFINES
 *********************************/
#define BAUDRATE              115200   // Serial Speed
#define SERIAL_TIMEOUT        5000     // Serial Timeout 5s
#define FLIP_DURATION         500      // 500 microseconds
#define FLIP_PAUSE_DURATION   250      // 250 microseconds
#define YELLOW                true
#define BLACK                 false

#define DEBUG                 1
#define DEBUG_INIT            1        // Debug Output during "setup"                 1
#define DEBUG_UPDATE_DISPLAY  0        // Debug Output during "updateDisplay"
#define DEBUG_SEED            1        // Debug Seeding the Random Generator 
#define DEBUG_SET_DOT         0        // Debug Output during "setDot"
#define DEBUG_DRAW_CHAR       0        // Debug Output during "drawChar"
#define DEBUG_SCROLLTEST      0        // Debug Output during "scrollText"
#define DEBUG_PASSHINT        1        // Debug Output during "f_password"

#define DBG                if(DEBUG)Serial 
#define DBG_INIT           if(DEBUG_INIT)Serial 
#define DBG_UPDATE_DISPLAY if(DEBUG_UPDATE_DISPLAY)Serial 
#define DBG_SEED           if(DEBUG_SEED)Serial 
#define DBG_SET_DOT        if(DEBUG_SET_DOT)Serial 
#define DBG_DRAW_CHAR      if(DEBUG_DRAW_CHAR)Serial  
#define DBG_SCROLLTEST     if(DEBUG_SCROLLTEST)Serial  
#define DBG_PASSHINT       if(DEBUG_PASSHINT)Serial  

#define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))

/*********************************
 * Global Vars
 *********************************/
// Lookup Table to tansform Demux-Address to a selected Row-Driver
uint8_t ROW_TABLE_OFF[8] = {13, 12, 15, 14, 9, 8, 11, 10};      // Board from mezgrman
uint8_t ROW_TABLE_ON[8]  = {0, 2, 1, 3, 4, 5, 6, 7};            // Board from mezgrman

// Lookup Table to tansform Enable of single Panel ID to GPIO Port
uint8_t E_LINES[5] = {E1, E2, E3, E4, E5};

// Framebuffer
uint16_t  thisFrame[MATRIX_WIDTH] = {0};
uint16_t  nextFrame[MATRIX_WIDTH] = {0};

bool g_stopOutput;

// Fot Related
GFXfont *gfxFont;      // actual selected font for drawText() 
uint8_t g_cursorX;     // X-Position of Cursor for drawText() 
uint8_t g_cursorY;     // Y-Position of Cursor for drawText() 
bool    g_firstrun;    // to do only things in first loop()
uint8_t g_hintcnt;     // Password Hint Counter

#define MAX_HINTS
// Definition der Byte-Arrays
const PROGMEM byte byteArray1[] = {0x01, 0x02, 0x03, 0x04};
const PROGMEM byte byteArray2[] = {0x11, 0x12, 0x13, 0x14};
const PROGMEM byte byteArray3[] = {0x21, 0x22, 0x23, 0x24};

// Definition des Arrays mit den Byte-Arrays
const PROGMEM byte* const byteArrayArray[] = {byteArray1, byteArray2, byteArray3};


// TODO
bool matrixClean = false;
bool pixelInverting = false;
bool activeState = true;
bool quickUpdate = true;
enum SERIAL_STATUSES {
  SUCCESS = 0xFF,
  TIMEOUT = 0xE0,
  ERROR = 0xEE,
};


/* 
 * Schedule Reset for Arduino to enter Programming Mode
 */
void enterProgrammingMode() {
  wdt_enable(WDTO_15MS);
  for (;;);
}


/**************************************************************************/
/*!
    @brief Receive a Bitmap over Serial Port
    @TODO: Change to use the Global Framebuffer: nextFrame[MATRIX_WIDTH]
*/
/**************************************************************************/
/*
void receiveBitmap() {
  // Receive number of columns
  uint8_t numuint8_ts;
  if (!readuint8_tsOrTimeoutError(&numuint8_ts, 1)) return;
  // Receive bitmap data
  uint8_t serBuf[numuint8_ts];
  unsigned int newBitmap[numuint8_ts / 2];
  if (!readuint8_tsOrTimeoutError(serBuf, numuint8_ts)) return;
  // Piece the uint8_ts together
  for (int i = 0; i < numuint8_ts; i += 2) {
    newBitmap[i / 2] = (serBuf[i] << 8) + serBuf[i + 1];
  }  
}
*/


/*
 * Send Response over Serial 
 * @param[out] status Response Code to be send
 */
void serialResponse(uint8_t status) {
  Serial.write(status);
}
 

/*********************************************************************
 * Read N Byes from Serial
 * - aboad if timeout
 * @param[out] uint8_t  Buffer where the received baytes shall be stored
 * @param[in] length Number of uint8_ts to be received
 * @returns true: Number of uint8_ts have been received, false: timeout
 *********************************************************************/
bool readuint8_tsOrTimeout(uint8_t* buffer, int length) {
  int startTime = millis();
  for (int n = 0; n < length; n++) {
    while (!Serial.available()) {
      int timeTaken = millis() - startTime;
      if (timeTaken >= SERIAL_TIMEOUT || timeTaken < 0) { // Second test in case millis rolled over
        return false;
      }
    }
    buffer[n] = Serial.read();
  }
  return true;
}

/*********************************************************************
 *
 * Read N Byes from Serial
 * - aboad if timeout
 * - on Timeout: send Errorcode over Serial
 * @param[out] uint8_t  Buffer where the received baytes shall be stored
 * @param[in] length Number of uint8_ts to be received
 * @returns true: Number of uint8_ts have been received, false: timeout
 *********************************************************************/
bool readuint8_tsOrTimeoutError(uint8_t* buffer, int length) {
  bool success = readuint8_tsOrTimeout(buffer, length);
  if (!success) {
    serialResponse(TIMEOUT);
  }
  return success;
}


/**************************************************************************/
/*!
    @brief Frame: Display TELNET - KLARTEXT REDEN
    @param[in] mode true:on, false: off 
*/
/**************************************************************************/
void f_telnet(void) {
  DBG.print("f_telnet ... ");
  clearDisplayBuffer(BLACK);
  updateDisplay(false,2,0);       
  // TELNET
  // setFont(&FlipDot16);
  setFont(&FlipDot14);  
  g_cursorX = 0;
  g_cursorY = 15;                        
  drawText(g_cursorX, g_cursorY, "TELNET", YELLOW);
  wipein(3,0);    
  updateDisplay(false,2,0);       
  delayWithSerialBreak(1000);
  // KLARTEXT REDEN
  setFont(&FlipDot8);  
  g_cursorX = 60;
  g_cursorY = 9;
  drawText(g_cursorX, g_cursorY, "KLARTEXT", YELLOW);
  g_cursorX = 70;
  g_cursorY = 17;
  drawText(g_cursorX, g_cursorY, "REDEN", YELLOW);            
  updateDisplay(true,4,0);   
  delayWithSerialBreak(5000);
  DBG.println("done.");
}

/**************************************************************************/
/*!
    @brief Frame: Password Hint
    @param[in] mode true:on, false: off 
*/
/**************************************************************************/
void f_password(void) {
  char buffer[HINT_LENGTH];  // Buffer to store current hint
  const char* constbuf= buffer;  
  char temp;                 // register used when swapping two values 
  uint8_t i;                 // counter
  uint8_t j;                 // index
  DBG.print("f_password ... ");
  clearDisplayBuffer(BLACK);  
  setFont(&FlipDot16);      
  g_cursorX = 2;
  g_cursorY = 15;
  drawText(g_cursorX, g_cursorY, "Password Hint", true);           
  updateDisplay(false,5,0);  
  delayWithSerialBreak(3000);
  clearDisplayBuffer(0);        
  setFont(&FlipDot8);
  g_cursorX = 2;
  g_cursorY = 8;
  drawText(g_cursorX, g_cursorY, "Characters NOT used:", true);            
  updateDisplay(false,5,0);
  delayWithSerialBreak(3000);  
  g_cursorX = 27;
  g_cursorY = 16;
  // Display Hints  
  g_hintcnt++;
  if (g_hintcnt > HINT_NUMS - 1) {
    g_hintcnt = 0;
  }
  DBG_PASSHINT.print("Hint ");
  DBG_PASSHINT.print(g_hintcnt);
  DBG_PASSHINT.print(": ");
  // Get Hint from Progmem to buffer  
  strcpy_P (buffer, (char*)pgm_read_word(&(hints[g_hintcnt]))); 
  DBG_PASSHINT.println(&buffer[0]);    
  // the buffer with Fisher-Yates shuffle algorithm 
  for (i=HINT_LENGTH-1; i>0; --i) {
    j = random(i+1);
    // swap array[i] and array[j]
    temp = buffer[i];
    buffer[i] = buffer[j];
    buffer[j] = temp;
  }
  DBG_PASSHINT.print("schuffeled: ");  
  DBG_PASSHINT.print(constbuf);    
  drawText(g_cursorX, g_cursorY, constbuf, true, 10);    
  DBG_PASSHINT.println("");  
  updateDisplay(true,2,0);
  delayWithSerialBreak(1000);
  clearDisplay(6,1,0);  
  delayWithSerialBreak(5000);
  DBG.println("done.");    
}

/**************************************************************************/
/*!
    @brief 37c3 UNLOCKED
*/
/**************************************************************************/
void f_unlocked(void) {      
  DBG.print("f_unlocked ... ");
  clearDisplayBuffer(BLACK);    
  setFont(&FlipDot14);          
  g_cursorX = 26;
  g_cursorY = 15;   
  drawText(g_cursorX, g_cursorY, "37C3", YELLOW,7);                 
  updateDisplay(false,5,0);
  delayWithSerialBreak(3000);      
  clearDisplayBuffer(YELLOW);    
  g_cursorX = 4;
  g_cursorY = 15;   
  drawText(g_cursorX, g_cursorY, "UNLOCKED", BLACK, 2);
  updateDisplay(false,7,0);
  delayWithSerialBreak(5000);      
  DBG.println("done.");
}


/**************************************************************************/
/*!
    @brief Announce Telnet Challenge    
*/
/**************************************************************************/
void f_challenge(void) {
  DBG.println("f_challenge ... ");
  clearDisplayBuffer(BLACK);    
  setFont(&FlipDot16);      
  g_cursorX = 0;
  g_cursorY = 15;   
  drawText(g_cursorX, g_cursorY, "WINKEKATZEN", YELLOW);
  updateDisplay(false,5 ,0);      
  delayWithSerialBreak(3000);      
  clearDisplayBuffer(BLACK);    
  setFont(&FlipDot14);      
  g_cursorX = 5;
  g_cursorY = 15;   
  drawText(g_cursorX, g_cursorY, "CHALLENGE", YELLOW, 1);      
  updateDisplay(true,7,0);
  delayWithSerialBreak(3000);            
  // ---------------
  clearDisplayBuffer(BLACK);    
  setFont(&FlipDot8);      
  g_cursorX = 4;
  g_cursorY = 8;   
  drawText(g_cursorX, g_cursorY, "Help the cat to wave", YELLOW, 0);
  updateDisplay(true,7,0);
  delayWithSerialBreak(3000);         
  g_cursorX = 4;
  g_cursorY = 17;   
  drawText(g_cursorX, g_cursorY, "& win a free T-Shirt", YELLOW, 0);
  g_cursorX++;
  drawText(g_cursorX, g_cursorY, "!", YELLOW, 0);
  updateDisplay(true,5,0);
  delayWithSerialBreak(3000);    
  // ---------------
  clearDisplayBuffer(BLACK);    
  setFont(&FlipDot8);      
  g_cursorX = 8;
  g_cursorY = 12;   
  drawText(g_cursorX, g_cursorY, "Start by looking at", YELLOW, 0);
  updateDisplay(true,5,0);
  delayWithSerialBreak(3000);    
  clearDisplayBuffer(BLACK);    
  g_cursorX = 8;
  g_cursorY = 15;   
  setFont(&FlipDot14);      
  drawText(g_cursorX, g_cursorY, "HER EYES !!!", YELLOW, 0);
  wipein(3,0);    
  delayWithSerialBreak(5000);    
  DBG.println("done.");
}

/**************************************************************************/
/*!
    @brief Announce Hot Wire
*/
/**************************************************************************/
void f_hotWire(void) {
  DBG.println("f_hotWire ... ");
  clearDisplayBuffer(BLACK);    
  g_cursorX = 15;
  g_cursorY = 8;   
  setFont(&FlipDot8);
  drawText(g_cursorX, g_cursorY, "TEAM  TELNET", YELLOW, 1);      
  g_cursorX = 6;
  g_cursorY = 17;   
  drawText(g_cursorX, g_cursorY, "KLARTEXT REDEN", YELLOW, 1);      
  updateDisplay(false, 5 ,0);      
  delayWithSerialBreak(3000);      
  // -- 
  clearDisplayBuffer(BLACK);    
  g_cursorX = 20;
  g_cursorY = 8;   
  setFont(&FlipDot8);
  drawText(g_cursorX, g_cursorY, "PROUDLY", YELLOW, 1);         // 30
  g_cursorX = 30;
  g_cursorY = 17;   
  drawText(g_cursorX, g_cursorY, "PRESENTS", YELLOW, 1);        // 11
  g_cursorY--;   
  drawText(g_cursorX, g_cursorY, ":", YELLOW, 1);        // 11
  updateDisplay(false, 5 ,0);      
  delayWithSerialBreak(3000);      
  // -- 
  clearDisplayBuffer(YELLOW);      
  g_cursorX = 9;
  g_cursorY = 15;   
  setFont(&FlipDot14);
  drawText(g_cursorX, g_cursorY, "HOT", BLACK, 1);      
  g_cursorX += 2;
  drawText(g_cursorX, g_cursorY, "-", BLACK, 1);      
  g_cursorX += 2;
  drawText(g_cursorX, g_cursorY, "WIRE", BLACK, 1);      
  updateDisplay(false,5 ,0);     
  delayWithSerialBreak(2500);       
  // -- 
  clearDisplayBuffer(YELLOW);      
  g_cursorX = 5;
  g_cursorY = 15;   
  drawText(g_cursorX, g_cursorY, "CHALLENGE", BLACK, 1);      
  updateDisplay(true,7,0);
  delayWithSerialBreak(2500);            
  // ---------------
  clearDisplayBuffer(BLACK);    
  setFont(&FlipDot8);      
  g_cursorX = 6;
  g_cursorY = 8;   
  drawText(g_cursorX, g_cursorY, "Fastest Player with", YELLOW, 0);  
  g_cursorX = 9;
  g_cursorY = 16;   
  drawText(g_cursorX, g_cursorY, "0 Errors each day", YELLOW, 0);
  updateDisplay(true,7,0);
  delayWithSerialBreak(3000);         
  // ---------------
  clearDisplayBuffer(BLACK);      
  g_cursorX = 5;
  g_cursorY = 16;   
  setFont(&FlipDot8);      
  drawText(g_cursorX, g_cursorY, "wins a ", YELLOW, 0);    
  setFont(&FlipDot14);      
  g_cursorY = 15;   
  drawText(g_cursorX, g_cursorY, "T", YELLOW, 0);  
  g_cursorX -= 2;
  drawText(g_cursorX, g_cursorY, "-Shirt", YELLOW, 0);    
  g_cursorX += 2;
  drawText(g_cursorX, g_cursorY, "!", YELLOW, 0);    
  wipein(3,0);
  delayWithSerialBreak(5000);    
  Serial.println("done.");
}

/*******************************************
 * SERIAL
 *******************************************/
void mySerialCommunication() {  
  if (!Serial.available()) return;
  // Check for start uint8_t
  uint8_t mykey;  
  if (!readuint8_tsOrTimeout(&mykey, 1)) return;      
  switch (mykey) {
    // "0" 
    case 48:            
      f_telnet();
      break;    
    // "1" Password
    case 49:                
      f_password();      
      break;
    // "2" Challenge
    case 50:                
      f_challenge();      
      break;
    // "3" scrollText
    case 51:  
      f_hotWire();
      break;
    // "4" scrollText
    case 52:        // ---------------------
      // scrollText("WINKEKATZEN CHALLENGE", 15, 150, YELLOW);           
      // scrollText("TELNET - KLARTEXT REDEN", 15, 150, YELLOW);               
      scrollText("HELP THE CAT TO WINK AND WIN A FREE T-SHIRT", 15, 0, YELLOW);       
      scrollText("PLAY THE HOT-WIRE.  FINISH WITH 0 ERRORS.  DAILY BEST TIME WINS A FREE T-SHIRT", 15, 0, YELLOW);             
      break;
    // "5" ON / OFF
    case 53:  
      for (int i=0; i<100; i++) {
        clearDisplay(2, BLACK, 0);  
        delayWithSerialBreak(100);      
        if (g_stopOutput) {break;}        
        delayWithSerialBreak(100);      
        if (g_stopOutput) {break;}
        clearDisplay(2, YELLOW, 0);          
      }
      break;      
    // "r" speed diagonals
    case 114:       
      updateDisplay(false,0,0);          
      for (uint8_t j=0; j<10; j++){
        setTestFrame(2);
        updateDisplay(true,2,0);          
        setTestFrame(3);
        updateDisplay(true,0,0);          
      }          
      break; 
    // "c" Clear Display
    case 99:
      clearDisplay(2, false, 0);                      
      break; 
    // "d" Displaytest: Flip all Dots ON/OFF
    case 100:
      for (uint8_t j=0; j<2; j++){
        uint8_t i = j*10;
        for (uint8_t m=0; m<4; m++){          
          uint8_t mode = m*2;
          clearDisplay(mode, true, i);
          clearDisplay(mode, false, i);
        }
      }
      break;  
    // "g" Quick Testframes
    case 103:             
      updateDisplay(false,4,0);          
      for (uint8_t j=0; j<5; j++){
        setTestFrame(j);
        updateDisplay(true,2,0);          
      }    
      break;         
    // "s" Slow Testframes
    case 115:             
      updateDisplay(false,2,0);          
      for (uint8_t j=0; j<5; j++){
        setTestFrame(j);
        updateDisplay(false,0,0);          
      }      
      break; 
    // "L" LED ON
    case 76: 
      //switchLedOn();
      break;     
    // "l" LED OFF      
    case 108: 
      //switchLedOff();
      break;            
    // Print Keycode if not handled
    default:
      DBG.print("received: ");
      DBG.println(mykey);
      break;               
    }    

  if (g_stopOutput) {
    DBG.println("STOP OUTPUT");
    clearSerialBuffer();
    g_stopOutput = false;
  }
}

/*
 * Delay ms, only if no chars in serial buffer
 */
bool delayWithSerialBreak(uint32_t ms) {
  uint32_t start;
  start = millis();
  while ( (millis() - start) < ms) {
    if (Serial.available()) {      
      return true;
    }
  }
  return false;
}

/*
 * Clear the Serial Buffer 
 */
void clearSerialBuffer(void) {
  while (Serial.available() > 0) {
    Serial.read();
  }
}

void movingBlock(uint8_t pos) {
  uint8_t x;
  uint8_t blocksize = 2;
  if ((pos + blocksize -1) < MATRIX_WIDTH) {
    for (x = 0; x < MATRIX_WIDTH; x++) {            
      if ((x >= pos) && (x < pos + blocksize)) {
        nextFrame[x] = 0x0ff0;
      } else {
        nextFrame[x] = 0x00;
      }
    }
  }
}

void setTestFrame(uint8_t type) {
  uint8_t x;
  for (x = 0; x < MATRIX_WIDTH; x++) {      
    switch (type) {
      case 0: // clear
        nextFrame[x] = 0x00;
        break;
      case 1: // all ON
        nextFrame[x] = 0xffff;
        break;
      case 2: // Diagonal
        nextFrame[x] = (1 << (x % 16) );
        break;
      case 3: // binary
        nextFrame[x] = (0x8000 >> (x % 16) );
        break;      
      case 4: // binary
        nextFrame[x] = (x ^ 0xff) + (x << 8);
        break;          
    }
  }
}

/*******************************************
 * DISPLAY
 *******************************************/

/**************************************************************************/
/*!
    @brief Read GFXglyph fromProgmem
    @param  gfxFont  The GFXfont object
    @param  c        Character Index to be read
*/
/**************************************************************************/
inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
  return &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c]);
}

/**************************************************************************/
/*!
    @brief Read GFXfont from Progmem
    @param  gfxFont  The GFXfont object    
*/
/**************************************************************************/
inline uint8_t *pgm_read_bitmap_ptr(const GFXfont *gfxFont) {
  return (uint8_t *)pgm_read_pointer(&gfxFont->bitmap);
}

/**************************************************************************/
/*!
    @brief Set the font to display when print()ing, either custom or default
    @param  f  The GFXfont object, if NULL use built in 6x8 font
*/
/**************************************************************************/
void setFont(const GFXfont *f) {
  gfxFont = (GFXfont *)f;
}


/**************************************************************************/
/*!
    @brief    Scroll a Char-Array
    @param    text[c]     The 8-bit font-indexed character (likely ascii)
    @param    y           Y-Offset
    @param    frameDelay  Scroll delay 
    @param    color       true: yellow, false: black    
*/
/**************************************************************************/
void scrollText(const char text[], uint8_t y, uint16_t frameDelay, bool color) {    
  uint8_t c;
  // uint8_t x;
  // uint8_t ci;
  uint8_t xi;
  uint8_t actChar;
  uint8_t actXPos;     
  uint8_t emptyFrame;
  uint32_t frameStart;  
  if (g_stopOutput) {return;}
  DBG_SCROLLTEST.print("scrollText: ");  
  DBG_SCROLLTEST.print(" - y:");
  DBG_SCROLLTEST.print(y);      
  DBG_SCROLLTEST.print(" - frameDelay: ");
  DBG_SCROLLTEST.print(frameDelay);
  DBG_SCROLLTEST.print("- color: ");
  DBG_SCROLLTEST.print(color);
  DBG_SCROLLTEST.println(".");
  DBG_SCROLLTEST.println("Start Scrolling in");  
  clearDisplayBuffer(!color);  
  g_cursorY = y;    
  actXPos = MATRIX_WIDTH - 1;
  actChar = 0;
  emptyFrame = false;
  while (!emptyFrame) {    
    if (g_stopOutput) {return;}
    frameStart = millis(); 
    emptyFrame = true;
    DBG_SCROLLTEST.print(" - <<");    
    // Scroll Framebuffer 1 Column left
    for (xi = 1; xi < MATRIX_WIDTH; xi++) {
      nextFrame[xi-1] = nextFrame[xi];    
      if (nextFrame[xi] != 0) {
        emptyFrame = false;
      }      
    }
    // Set Last Collumn according to color
    if (color){
      nextFrame[MATRIX_WIDTH - 1] = 0x0000;
    } else {
      nextFrame[MATRIX_WIDTH - 1] = 0xffff;
    }
    // Get actual Char, which has to be printed to the  Matrix
    c = (uint8_t) text[actChar];            
    // if there is a Char
    if (c != 0){
      // X-Position where to print it
      g_cursorX = actXPos--;      
      DBG_SCROLLTEST.print(" - #:");      
      DBG_SCROLLTEST.print(actChar);
      DBG_SCROLLTEST.print(" - ");      
      DBG_SCROLLTEST.print((char) c);      
      DBG_SCROLLTEST.print(" - X:");
      DBG_SCROLLTEST.print(g_cursorX);    
      drawChar(g_cursorX , g_cursorY, c, color);
      emptyFrame = false;
      DBG_SCROLLTEST.print(" - X after:");
      DBG_SCROLLTEST.print(g_cursorX);      
      // Did the current char fit completely to the Matrix?
      if (g_cursorX < MATRIX_WIDTH) {   
        // yes: Set Pointer to next Char and set new X-Position
        actChar++;              
        actXPos = g_cursorX - 1;        
        DBG_SCROLLTEST.print(" - Char fits Complete to Matrix, goto next Char:");
        DBG_SCROLLTEST.print((char)text[actChar]);
        DBG_SCROLLTEST.print(" - x(now):");
        DBG_SCROLLTEST.print(g_cursorX);
        DBG_SCROLLTEST.print(" - x(next loop):");
        DBG_SCROLLTEST.print(actXPos);            
      }
      DBG_SCROLLTEST.println(".");      
    }
    updateDisplay(true,2,0);  
    // Pause
    while ( (millis() - frameStart) < frameDelay) {
      if (Serial.available()) {      
        g_stopOutput = true;
        return;
      }
    }  
  }  
  DBG_SCROLLTEST.print("finished");
}

/**************************************************************************/
/*!
    @brief    Print a Char-Array
    @param    x   Bottom left corner x coordinate
    @param    y   Bottom left corner y coordinate
    @param    text[c]   The 8-bit font-indexed character (likely ascii)
    @param    color   true: yellow, false: black    
*/
/**************************************************************************/
void drawText(uint8_t x, uint8_t y, const char text[], bool color) {    
  drawText(x, y, text, color, 0);
}    

/**************************************************************************/
/*!
    @brief    Print a Char-Array
    @param    x   Bottom left corner x coordinate
    @param    y   Bottom left corner y coordinate
    @param    text[c]   The 8-bit font-indexed character (likely ascii)
    @param    color   true: yellow, false: black    
    @param    space   horizontal spacing 
*/
/**************************************************************************/
void drawText(uint8_t x, uint8_t y, const char text[], bool color, uint8_t spacing) {    
  uint8_t c;
  uint8_t i;
  i = 0;
  c = (uint8_t) text[i++];
  g_cursorX = x;
  g_cursorY = y;
  while ((g_cursorX < (MATRIX_WIDTH - 1)) && (c != 0)) {    
    drawChar(g_cursorX, g_cursorY, c, color);
    c = (uint8_t) text[i++];
    g_cursorX += spacing;
  }
}

/**************************************************************************/
/*!
    @brief   Draw a single character
    @param    x   Bottom left corner x coordinate
    @param    y   Bottom left corner y coordinate
    @param    c   The 8-bit font-indexed character (likely ascii)
    @param    color   true: yellow, false: black    
*/
/**************************************************************************/
void drawChar(uint8_t x, uint8_t y, uint8_t c, bool color) {    
  // Debug  
  DBG_DRAW_CHAR.print("drawChar:");  
  DBG_DRAW_CHAR.print(c);
  DBG_DRAW_CHAR.print(" - x:");
  DBG_DRAW_CHAR.print(x);
  DBG_DRAW_CHAR.print(" - y:");
  DBG_DRAW_CHAR.print(y);
  DBG_DRAW_CHAR.print("- color:");
  DBG_DRAW_CHAR.print(color);
  DBG_DRAW_CHAR.println("");          
  // draw
  c -= (uint8_t)pgm_read_byte(&gfxFont->first);        // Get Index of Char in gfxFont  
  GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c);    // read Position of Glyph in gfxFont
  uint8_t *bitmap = pgm_read_bitmap_ptr(gfxFont);      // read Position of Bitmap in gfxFont
  uint16_t bo = pgm_read_word(&glyph->bitmapOffset);   
  uint8_t   w = pgm_read_byte(&glyph->width);
  uint8_t   h = pgm_read_byte(&glyph->height);
  int8_t xadv = pgm_read_byte(&glyph->xAdvance);
  int8_t   xo = pgm_read_byte(&glyph->xOffset);        // this Character X Offest 
  int8_t   yo = pgm_read_byte(&glyph->yOffset);        // this Character Y Offest 
  uint8_t xx = 0;                                      // X-Counter
  uint8_t yy = 0;                                      // Y-Counter
  uint8_t bits = 0;
  uint8_t bit = 0;
  uint8_t dotx = 0;                                    // Dot X-Position
  uint8_t doty = 0;                                    // Dot Y-Position
  g_cursorX += xadv;
  // debug  
  DBG_DRAW_CHAR.print(" - bo:");  
  DBG_DRAW_CHAR.print(bo);
  DBG_DRAW_CHAR.print(" - w:");
  DBG_DRAW_CHAR.print(w);
  DBG_DRAW_CHAR.print(" - h:");
  DBG_DRAW_CHAR.print(h);
  DBG_DRAW_CHAR.print(" - xo:");
  DBG_DRAW_CHAR.print(xo);
  DBG_DRAW_CHAR.print(" - yo:");
  DBG_DRAW_CHAR.print(yo);
  DBG_DRAW_CHAR.println("");          


  for (yy = 0; yy < h; yy++) {    
    DBG_DRAW_CHAR.print(" - yy: ");  
    DBG_DRAW_CHAR.println(yy);  
    for (xx = 0; xx < w; xx++) {
      DBG_DRAW_CHAR.print(" - xx: ");  
      DBG_DRAW_CHAR.println(xx);      
      if (!(bit++ & 7)) {
        bits = pgm_read_byte(&bitmap[bo++]);
        DBG_DRAW_CHAR.print(" - bits: ");  
        DBG_DRAW_CHAR.println(bits);
      }
      if (bits & 0x80) {          
        dotx = x + xo + xx;
        doty = y + yo + yy;        
        if ((dotx < MATRIX_WIDTH) && (doty < MATRIX_WIDTH)) {
          if (color){
            bitSet(nextFrame[dotx], MATRIX_HEIGHT - (doty));
            DBG_DRAW_CHAR.print(" - setDot");              
          }else{
            bitClear(nextFrame[dotx], MATRIX_HEIGHT - (doty));
            DBG_DRAW_CHAR.print(" - clearDot");              
          }
        }
        // debug       
        DBG_DRAW_CHAR.print(" - x:");
        DBG_DRAW_CHAR.print(dotx);
        DBG_DRAW_CHAR.print(" - y:");
        DBG_DRAW_CHAR.print(y + yo + yy);
        DBG_DRAW_CHAR.print("- color:");
        DBG_DRAW_CHAR.print(color);
        DBG_DRAW_CHAR.println("");          
      }
      bits <<= 1;
    }
  }
}

/**************************************************************************/
/*!
    @brief  wipein 
            - wipe nextFrame in 
    @param[in] w width of cursor
    @param[in] dotdelay delay after every Dot [ms]
*/
/**************************************************************************/
void wipein(uint8_t w, uint16_t coldelay) {
  uint8_t x;  
  uint8_t y;  
  bool inverted = false;

  // Don't even Start it stopOutput is true
  if (g_stopOutput) {return;}
  if (w == 0) {
    w=1;
    inverted = true;
  }  

  // MATRIX_WIDTH Steps
  for (x = 0; x < MATRIX_WIDTH + w; x++) {      
    for (y = 0; y < MATRIX_HEIGHT + w; y++) {     
      // Draw Cursor 
      if (x < MATRIX_WIDTH) {
        if (inverted) {
          switchDot(x, y, false);        
        }else{
          switchDot(x, y, true);        
        }        
      }
      // Overwrite old Cursor with data from nextFrame
      if (x > (w-1)) {
        if (bitRead(nextFrame[x-w], y)) {
          bitSet(thisFrame[x-w], y);
          if (inverted) {            
            switchDot(x-w, y, true);
          }
        } else {
          bitClear(thisFrame[x-w], y);
          if (!inverted) {          
            switchDot(x-w, y, false);
          }
        }     
      }  
    }
    // Column Delay
    if (coldelay != 0) {
      if( delayWithSerialBreak(coldelay)) {
        g_stopOutput = true;
        return;
      }
    } else {
      if (Serial.available()) {      
        g_stopOutput = true;
        return;
      }
    }
  }
  deselect();
}


/**************************************************************************/
/*!
    @brief  Update Display
            - set Sots according new Framebuffer 
    @param[in] quick: if true flip only changed dots
    @param[in] mode Mode:
                    - 0: Horizontal left to right (top to bottom)
                    - 1: Horizontal left to right (bottom to top)
                    - 2: Horizontal right to left (top to bottom)
                    - 3: Horizontal right to left (bottom to top)
                    - 4: Vertical   bottom to top (left to right)
                    - 5: Vertical   bottom to top (right to left)
                    - 6: Vertical   top to bottom (left to right)
                    - 7: Vertical   top to bottom (right to left)
    @param[in] dotdelay delay after every Dot [ms]
*/
/**************************************************************************/
void updateDisplay(bool quick, uint8_t mode, uint32_t dotdelay) {
  uint8_t col;    
  uint8_t row;
  uint8_t nCol;  
  uint8_t nRow;  
  bool innerRow;
  bool innerDir;
  bool outerDir;
#if DEBUG_UPDATE_DISPLAY  
  uint32_t t1;
  uint32_t t2;  
  float dotsPerSecond;
#endif
  
  // Don't even Start it stopOutput is true
  if (g_stopOutput) {return;}

  // Calc mode vars
  innerRow = mode < 4;
  innerDir = (mode % 2);
  outerDir = (mode/2) % 2;

  #if DEBUG_UPDATE_DISPLAY  
    DBG_UPDATE_DISPLAY.print("Update Display: - Delay: ");  
    DBG_UPDATE_DISPLAY.print(dotdelay);    
    DBG_UPDATE_DISPLAY.print("  Mode: ");  
    DBG_UPDATE_DISPLAY.print(mode);  
    DBG_UPDATE_DISPLAY.print(" - innerRow: - ");  
    DBG_UPDATE_DISPLAY.print(innerRow);  
    DBG_UPDATE_DISPLAY.print(" - innerDir: - ");  
    DBG_UPDATE_DISPLAY.print(innerDir);  
    DBG_UPDATE_DISPLAYerial.print(" - outerDir: - ");  
    DBG_UPDATE_DISPLAY.println(outerDir);  
    t1 = millis();
  #endif
  
  if (innerRow) {
    // Inner Loop: Rows
    for (nCol = 0; nCol < MATRIX_WIDTH; nCol++) {      
      if (outerDir) {
        col = nCol;
      } else {
        col = MATRIX_WIDTH - nCol - 1;
      }
      for (nRow = 0; nRow < MATRIX_HEIGHT; nRow++) {      
        if (innerDir) {
          row = nRow;
        } else {
          row = MATRIX_HEIGHT - nRow - 1;
        }        
        setDot(col, row, !quick);
        if (dotdelay != 0) {
          if( delayWithSerialBreak(dotdelay)) {
            g_stopOutput = true;
            return;
          }
        } else {
          if (Serial.available()) {      
            g_stopOutput = true;
            return;
          }
        }
      }
    }    
  } else {
    // Inner Loop: Columns
    for (nRow = 0; nRow < MATRIX_HEIGHT; nRow++) {      
      if (outerDir) {
        row = nRow;
      } else {
        row = MATRIX_HEIGHT - nRow - 1;
      }      
      for (nCol = 0; nCol < MATRIX_WIDTH; nCol++) {
        if (innerDir) {
          col = nCol;
        } else {
          col = MATRIX_WIDTH - nCol - 1;
        }          
        setDot(col, row, !quick);
        if (dotdelay != 0) {
          if (delayWithSerialBreak(dotdelay)) {
            g_stopOutput = true;
            return;
          }
        } else {
          if (Serial.available()) {      
            g_stopOutput = true;
            return;
          }
        }
      }      
    }    
  }
  
  #if DEBUG_UPDATE_DISPLAY  
    t2 = millis();
    DBG_UPDATE_DISPLAY.print("###  from: ");  
    DBG_UPDATE_DISPLAY.print(t1);
    DBG_UPDATE_DISPLAY.print(" to: ");  
    DBG_UPDATE_DISPLAY.print(t2);
    DBG_UPDATE_DISPLAY.print(" - time: ");  
    DBG_UPDATE_DISPLAY.print(t2-t1);
    DBG_UPDATE_DISPLAY.print(" - ms");      
    // frames/s
    DBG_UPDATE_DISPLAY.print(" - frames/s: ");      
    dotsPerSecond = 1000 / (float)(t2-t1);  
    DBG_UPDATE_DISPLAY.println(dotsPerSecond);  
  #endif
  deselect();
}


/**************************************************************************/
/*!
    @brief  Clear Display
            - Set/Reset all dots
    @param[in] mode modus (see updateDisplay)
    @param[in] color true: yellow, false: black
    @param[in] dotdelay delay after every Dot [ms]
    @param[in] update send Framebuffer to FlipDot
*/
/**************************************************************************/
void clearDisplay(uint8_t mode, bool color, uint32_t dotdelay) {  
  clearDisplayBuffer(color);
  updateDisplay(false, mode, dotdelay);  
}

/**************************************************************************/
/*!
    @brief  Clear Display Buffer               
    @param[in] color true: yellow, false: black  
*/
/**************************************************************************/
void clearDisplayBuffer(bool color) {
  for (uint8_t x=0; x<MATRIX_WIDTH; x++) {
    if (color) {      
      nextFrame[x] = 0xffff;
    } else {      
      nextFrame[x] = 0x0000;      
    }
  }
}

/**************************************************************************/
/*!
    @brief Set column GPIOs for the FP2800 according the desired column (X)
    @param[in] column Column to be selected
*/
/**************************************************************************/
void selectColumn(uint8_t column) {
  uint8_t address;
  //  Reverse Address due to Panel Wireing
  column = MATRIX_WIDTH - column - 1;
  // Get address for local panel
  address = column % PANEL_WIDTH;
  // Calculate real FP2800 chip address due to Segment/Digit Adressing in FP2800
  address += (address / 7) + 1;
  // Sert GPIOs
  digitalWrite(COL_A0, address & 1);
  digitalWrite(COL_A1, address & 2);
  digitalWrite(COL_A2, address & 4);
  digitalWrite(COL_A3, address & 8);
  digitalWrite(COL_A4, address & 16);
}


/**************************************************************************/
/*!
    @brief  Select the specified Linedriver (y)
            - convert Row# and mode to Demuxer-Address
            - write Demuxer-Address
            - set the D-Line of Flip-Dot Module according to mode (black: 1, yellow: 0)
    @param[in] row  Row to be selected 0:lowest 
    @param[in] mode Dot On/Off? true: Select HIGHSIDE-Linedriver, false: Select LOWSIDE-Linedriver
*/
/**************************************************************************/
void selectRow(uint8_t row, bool mode) {
  uint8_t demuxVal;
  bool useUpperDemuxer;
  // Convert Row# to Demuxer-Value
  if (mode) {
    demuxVal = ROW_TABLE_ON[row % 8];  // ON
  }else{
    demuxVal = ROW_TABLE_OFF[row % 8]; // OFF
  }  
  // Write to Demuxer
  digitalWrite(ROW_A0, demuxVal & 1);
  digitalWrite(ROW_A1, demuxVal & 2);
  digitalWrite(ROW_A2, demuxVal & 4);
  digitalWrite(ROW_A3, demuxVal & 8);
  // Select Upper/Lower Demuxer  
  useUpperDemuxer = (row > 7);
  digitalWrite(ROWS_BOTTOM, useUpperDemuxer);
  digitalWrite(ROWS_TOP, !useUpperDemuxer);
  // Set the D line of the FP2800 chip according mode (highside for black, lowside for yellow)
  digitalWrite(D, !mode);
}


/**************************************************************************/
/*!
    @brief  Set everything to inactive
            - both Demuxer disabled (ROWS_BOTTOM and ROWS_TOP = HIGH)
            - set Demuxer-Address to 0x00
            - set Flipdot-Column-Address to 0x00
*/
/**************************************************************************/
/* 
 */
void deselect(void) {
  //  both Demuxer disabled (ROWS_BOTTOM and ROWS_TOP = HIGH)
  digitalWrite(ROWS_BOTTOM, HIGH);
  digitalWrite(ROWS_TOP, HIGH);
  // Demuxer-Address to 0x00
  digitalWrite(ROW_A0, LOW);
  digitalWrite(ROW_A1, LOW);
  digitalWrite(ROW_A2, LOW);
  digitalWrite(ROW_A3, LOW);
  // Flipdot-Column-Address to 0x00
  digitalWrite(COL_A0, LOW);
  digitalWrite(COL_A1, LOW);
  digitalWrite(COL_A2, LOW);
  digitalWrite(COL_A3, LOW);
  digitalWrite(COL_A4, LOW);
}


/**************************************************************************/
/*!
    @brief  Flip the current selected DOT
            - Demuxer-Address has to be set before
            - Flipdot-Column-Address has to be set before
            - both Demuxer disabled (ROWS_BOTTOM and ROWS_TOP = HIGH)
    @param[in] panel Panel# where the dot should be flipped    
*/
/**************************************************************************/
void flip(uint8_t panel) {    
  uint8_t enablePin;
  // Convert Panel# to Enable Pin of desired Panel
  enablePin = E_LINES[PANEL_LINES[panel]];
  // enable
  digitalWrite(enablePin, HIGH);
  // wait
  delayMicroseconds(FLIP_DURATION);
  // disable
  digitalWrite(enablePin, LOW);
  // wait
  delayMicroseconds(FLIP_PAUSE_DURATION);
}


/**************************************************************************/
/*!
    @brief Set state of single Dot
    @param[in] x x-coordinate 0 to MATRIX_WIDTH
    @param[in] y y-coordinate 0 to MATRIX_HEIGHT 
    @param[in] mode true:on, false: off 
*/
/**************************************************************************/
void switchDot(uint8_t x, uint8_t y, bool mode) {      
  uint8_t panel;
  if ((x < MATRIX_WIDTH) && (y < MATRIX_HEIGHT)){
    panel = x / PANEL_WIDTH;
    selectRow(y, mode);            
    selectColumn(x);            
    flip(panel);  
  }
}

/**************************************************************************/
/*!
    @brief  Switch a single Dot to become state of new framebuffer
            - get actual state from thisFrame[]
            - flip only if state in nextFrame[] is different - EXCEPT froce is true
            - store actual state in thisFrame[]    
    @param[in] x x-coordinate 0 to MATRIX_WIDTH
    @param[in] y y-coordinate 0 to MATRIX_HEIGHT 
    @param[in] force flip dot even if Dot is unchanged (thisFrame[this dot] == nextFrame[this dot])
*/
/**************************************************************************/
void setDot(uint8_t x, uint8_t y, bool force) {
  bool oldval;
  bool newval;  
  // Get actual state  
  oldval = bitRead(thisFrame[x], y);
  newval = bitRead(nextFrame[x], y);
  #if DEBUG_SET_DOT
    DBG_SET_DOT.print("Setdot");  
    DBG_SET_DOT.print(" - x:");
    DBG_SET_DOT.print(x);
    DBG_SET_DOT.print(" - y:");
    DBG_SET_DOT.print(y);
    DBG_SET_DOT.print("- force:");
    DBG_SET_DOT.print(force);
    DBG_SET_DOT.print(" - oldval:");
    DBG_SET_DOT.print(oldval);  
    DBG_SET_DOT.print(" - newval:");
    DBG_SET_DOT.print(newval);
  #endif
  if ((oldval != newval) || force) {    
    DBG_SET_DOT.print("  - FLIP");    
    switchDot(x, y, newval);
    // Store newval to thisframe
    if (newval) {
      bitSet(thisFrame[x], y);
    } else {
      bitClear(thisFrame[x], y);
    }  
  }    
  DBG_SET_DOT.println(".");  
}


/**************************************************************************/
/*!
    @brief Switch LED-Stripe 
    @param[in] mode true:on, false: off 
*/
/**************************************************************************/
void switchLed(bool mode) {      
  digitalWrite(LED, mode);  
}


/**************************************************************************/
/*!
    @brief Switch LED-Stripe ON
*/
/**************************************************************************/
void switchLedOn(void) {
  digitalWrite(LED, true);  
}

/**************************************************************************/
/*!
    @brief Switch LED-Stripe OFF
*/
/**************************************************************************/
void switchLedOff(void) {
  digitalWrite(LED, false);  
}


/**************************************************************************/
/*!
    @brief Initialize Global Vars
*/
/**************************************************************************/
void setupGlobalVars(void) {  
  // Initialize Global Vars  
  g_stopOutput = false;
  g_cursorX = 0;
  g_cursorY = 0;
  g_hintcnt = 0;
  g_firstrun = true;
  DBG_INIT.println("done.");
}

/**************************************************************************/
/*!
    @brief Initialize the Random Number Generator
*/
/**************************************************************************/
void setupRandomSeed(void) {    
  uint32_t seed;  
  seed = analogRead(A6);   
  DBG_SEED.println("Seed1: ");
  DBG_SEED.println(seed);
  seed = seed << 8;
  seed += analogRead(A7);  
  DBG_SEED.println("Seed2: ");
  DBG_SEED.println(seed);
  seed = seed << 8;
  seed += analogRead(A6);  
  DBG_SEED.println("Seed3: ");
  DBG_SEED.println(seed);
  seed = seed << 8;
  seed += analogRead(A7);    
  DBG_SEED.println("Seed4: ");  
  DBG_SEED.println(seed);  
  randomSeed(seed);    
  DBG_INIT.println("done.");
}

/**************************************************************************/
/*!
    @brief Setup GPIO
*/
/**************************************************************************/
void setupGPIO(void) {  
  // Set those two pins high before setting them as outputs, since they are connected to active-low inputs
  digitalWrite(ROWS_BOTTOM, HIGH);
  digitalWrite(ROWS_TOP, HIGH);
  // Set LED OFF
  digitalWrite(LED, LOW);
  // Set Pinmodes
  pinMode(ROW_A0, OUTPUT);
  pinMode(ROW_A1, OUTPUT);
  pinMode(ROW_A2, OUTPUT);
  pinMode(ROW_A3, OUTPUT);
  pinMode(COL_A0, OUTPUT);
  pinMode(COL_A1, OUTPUT);
  pinMode(COL_A2, OUTPUT);
  pinMode(COL_A3, OUTPUT);
  pinMode(COL_A4, OUTPUT);
  pinMode(ROWS_BOTTOM, OUTPUT);
  pinMode(ROWS_TOP, OUTPUT);
  pinMode(D, OUTPUT);
  pinMode(E1, OUTPUT);
  pinMode(E2, OUTPUT);
  pinMode(E3, OUTPUT);
  pinMode(E4, OUTPUT);
  pinMode(E5, OUTPUT);
  // LED  
  pinMode(LED, OUTPUT);    
  DBG_INIT.println("done.");
}


/**************************************************************************/
/*!
    @brief Send Menu over Serial
*/
/**************************************************************************/
void sendMenu(void) {     
  Serial.println("1: Moving Block");
  Serial.println("d: Displaytest: Flip all Dots ON/OFF");
  Serial.println("g: Quick Testframes");
  Serial.println("s: Slow Testframes");
  Serial.println("r: speed diagonals");
}


/**************************************************************************/
/*!
    @brief SETUP
*/
/**************************************************************************/
void setup(void) {
  Serial.begin(BAUDRATE);
  // Hello
  DBG.println("Flipdot Controller");
  DBG.println("------------------");
  // Setup GPIO
  DBG_INIT.print("Init GPIO ... ");
  setupGPIO();
  // Init Global Vars
  DBG_INIT.print("Init Global Vars ... ");
  setupGlobalVars();  
  // Init Random Number Generator
  DBG_INIT.println("Init Random Number Generator");
  setupRandomSeed() ;
  // Setup Completed  
  DBG_INIT.println("Init completed.");
  DBG_INIT.println("\n");
  
  // sendMenu();
  
  // Start Main Loop  
  delay(100);
  DBG.println("Staring Main Loop.");
}

/**************************************************************************/
/*!
    @brief endless Animation
    cycles all good animations forever.
    can be interupted b<y sending anny char over Serial
*/
/**************************************************************************/
void endlessAnimation(){
  while (!g_stopOutput) {
    f_telnet();    // TELNET KLARTEXT-REDEN"
    f_challenge(); // TELNET KLARTEXT-REDEN"
    f_unlocked();  // 37c3 Unlocked
    f_hotWire();    // 37c3 Unlocked  
    f_password();  // Password Hint  
  }
}


/**************************************************************************/
/*!
    @brief MAIN LOOP    
*/
/**************************************************************************/

void loop(void) {
  mySerialCommunication();  
  while (1) {
    f_password();
  }
  if (g_firstrun){
    g_firstrun = false;
    endlessAnimation();
  }
}
