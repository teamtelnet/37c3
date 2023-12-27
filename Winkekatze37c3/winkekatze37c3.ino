/*****************************************************
 * Winkekatze 37c3 
 *
 * (c) 2023 Dario Carluccio
 * 
 * CPU-Board: Wemos D1 mini 
 * 
 * Arduino Settings:
 *    Board:  LOLIN (Wemos) D1 mini (clone) 
 *            esp8266:esp8266:d1_mini_clone
 *    Freq:   80MHz
 *    Flash:  4M (3M SPIFSS)
 *    Upload: 115200 
 * 
 * Connection:
 * - D2 - GPIO4: Servo        (Output)
 * - D4 - GPIO2: WS2812B LED  (Output) 2 LEDS
 * 
 * Function:
 * - Send Message with RGB-LEDs (Tap-code)
 * - wave hand (Controlled over Serial Interface)
 ******************************************************/

#include <Arduino.h>
#include <NeoPixelBus.h>
#include <Servo.h>

#define DEBUG_SETUP           0  // activate Setup Debug Messages
#define DEBUG_COMMAND         0  // command Debug Messages
#define DEBUG_HEARTBEAT       0  // Heartbeat Messages

#define SERVO_GPIO            4  // Servo on  D2 - GPIO4 
#define WS2812_PIXELS         2  // WS2812 on D4 - GPIO2 
#define colorSaturation     128
#define TAP_TIME            150
#define TAP_PAUSE_TIME      175
#define CHAR_PAUSE_TIME     350
#define STARTOVER_TIME     3000
#define HEARTBEAT_TIME    10000

#define DBG_SETUP if(DEBUG_SETUP)Serial 
#define DBG_COMMAND if(DEBUG_COMMAND)Serial 
#define DBG_HEARTBEAT if(DEBUG_HEARTBEAT)Serial  

// for Esp8266, pin is omitted and it uses D4-GPIO2 due to DMA hardware.
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1800KbpsMethod > strip(WS2812_PIXELS);

// create servo
Servo myservo;

// colors
RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor yellow(colorSaturation, colorSaturation, 0);
RgbColor lila(colorSaturation / 4, 0, colorSaturation );
RgbColor lila1(colorSaturation, 0, colorSaturation / 4);
RgbColor lila2(colorSaturation / 4, 0, colorSaturation);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

// global vars
unsigned int  g_cur_charpos;             // which char currently send
unsigned int  g_cur_bitpos;              // which BIT of the char is currently send
uint16_t      g_cur_eye_delay;           // time until next eye state has to be computed
bool          g_cur_eyestate;            // current eye state (ON/OFF)
unsigned long g_last_tap_millis;         // last time when a processEyes was called (to avoid delay)
unsigned long g_last_heartbeat_millis;   // last time when Heartbeat was called (to avoid delay) 
RgbColor g_cur_color_z;                  // Color for row-taps
RgbColor g_cur_color_s;                  // Color for column-taps

// const char* c_message =  "ABCDE FGHIJ K LMNOP QRSTU VWXYZ";
const char* c_message =     "PLEASE HELP ME TO WAVE AGAIN AND WIN A FREE TSHIRT FOR MORE INFORMATION VISIT THE TEAMTELNET GITHUB ACCOUNT";

// Klopfcode 
// https://de.wikipedia.org/wiki/Klopfcode
// Z: Tap for Row
// Z: Tap for Column
const char tap_A[] = "ZS";           // 1,1
const char tap_B[] = "ZSS";          // 1,2
const char tap_C[] = "ZSSS";         // 1,3
const char tap_D[] = "ZSSSS";        // 1,4
const char tap_E[] = "ZSSSSS";       // 1,5
const char tap_F[] = "ZZS";          // 2,1
const char tap_G[] = "ZZSS";         // 2,2
const char tap_H[] = "ZZSSS";        // 2,3
const char tap_I[] = "ZZSSSS";       // 2,4
const char tap_J[] = "ZZSSSSS";      // 2,5
const char tap_K[] = "ZSSS";         // 1,3 K = C
const char tap_L[] = "ZZZS";         // 3,1
const char tap_M[] = "ZZZSS";        // 3,2
const char tap_N[] = "ZZZSSS";       // 3,3
const char tap_O[] = "ZZZSSSS";      // 3,4
const char tap_P[] = "ZZZSSSSS";     // 3,5
const char tap_Q[] = "ZZZZS";        // 4,1
const char tap_R[] = "ZZZZSS";       // 4,2
const char tap_S[] = "ZZZZSSS";      // 4,3
const char tap_T[] = "ZZZZSSSS";     // 4,4
const char tap_U[] = "ZZZZSSSSS";    // 4,5
const char tap_V[] = "ZZZZZS";       // 5,1
const char tap_W[] = "ZZZZZSS";      // 5,2
const char tap_X[] = "ZZZZZSSS";     // 5,3
const char tap_Y[] = "ZZZZZSSSS";    // 5,4
const char tap_Z[] = "ZZZZZSSSSS";   // 5,5
const char tap_SPACE[] = "P";        // 

const char* const tap_table[] = 
{   
  tap_A,   tap_B,   tap_C,   tap_D,   tap_E,   tap_F,    //  0 -  5 
  tap_G,   tap_H,   tap_I,   tap_J,   tap_K,   tap_L,    //  6 - 11 
  tap_M,   tap_N,   tap_O,   tap_P,   tap_Q,   tap_R,    // 12 - 17
  tap_S,   tap_T,   tap_U,   tap_V,   tap_W,   tap_X,    // 18 - 23
  tap_Y,   tap_Z,                                        // 24 - 25
  tap_SPACE                                              // 26
  }; 

/****************************
 * Setup
 * - call all Init Functions
 *   - Serial Port
 *   - LEDs
 *   - WIFI Connection
 *   - MQTT Connection
 *   - Servo
 *   - Global Vars
 ****************************/
void setup() {
  init_serial();
  init_leds();
  init_servo();
  init_global_vars();
  DBG_SETUP.println("INIT completed, starting Main Loop...");
  DBG_SETUP.flush();
  delay(500);
}

/*****************************
 * Init: Serial Port
 * - 115200 BAUD
 * - Hello Message
 *****************************/
void init_serial(void) {
  Serial.begin(115200);
  while (!Serial); 
  DBG_SETUP.println();
  DBG_SETUP.println("37c3-Winkekatze");
  DBG_SETUP.println("===============");
  DBG_SETUP.println("Init...");
  DBG_SETUP.setTimeout(1000);
  delay(1000);
}

/*****************************
 * Init: LEDs
 * - init the WS2812 LEDs
 * - blink red for one second
 *****************************/
void init_leds(void) {
  // Init WS2812
  DBG_SETUP.print("... WS2812 LEDs connected to RxD...");
  strip.Begin();
  strip.Show();
  // set eyes red
  setEyes(red, red);
  DBG_SETUP.println("done.");
  DBG_SETUP.flush();
  delay(1000);
  setEyes(black, black);
  delay(500);
}

/**************
 * Init: Servo
 **************/
void init_servo(void) {  
  DBG_SETUP.print("... Servo connected to GPIO2...");
  myservo.attach(SERVO_GPIO);  
  // test Servo
  wink(3);           
  DBG_SETUP.println("done.");
  DBG_SETUP.flush();
}

/********************
 * Init: Global Vars
 ********************/
void init_global_vars(void)
{
  // Set global vars
  DBG_SETUP.print("... global vars...");
  g_cur_charpos = 0;               // Start with first char
  g_cur_bitpos = 0;                // Start with first BIT
  g_cur_eyestate = false;          // Start with exes OFF
  g_cur_eye_delay = 0;             // Start immidiate  
  g_last_tap_millis = millis();    // Start immidiatley
  g_cur_color_z = lila1;           // Start with purple eyes
  g_cur_color_s = green;           // Start with green eyes
  DBG_SETUP.println("done.");
  DBG_SETUP.flush();
}


/*********************************************
 * Test LEDs
 * - blink once in r, g, b
 *********************************************/
void test_LED(void) {
  strip.SetPixelColor(0, red);
  strip.SetPixelColor(1, red);
  strip.Show();
  delay(500);
  strip.SetPixelColor(0, green);
  strip.SetPixelColor(1, green);
  strip.Show();
  delay(500);
  strip.SetPixelColor(0, blue);
  strip.SetPixelColor(1, blue);
  strip.Show();
  delay(500);
  strip.SetPixelColor(0, black);
  strip.SetPixelColor(1, black);
  strip.Show();
  delay(500);
}



/*********************************************
 * Wink
 * wink n times, start with forward
 *********************************************/
void wink (uint8_t n) {  
  for (uint8_t i=0; i<n; i++){
    setEyes(blue, red);
    myservo.write(180);
    delay(250);
    setEyes(red, blue);
    myservo.write(0);
    delay(250);  
  }    
  setEyes(black, black);
}


/*********************************************
 * Set both eyes to same color 
 * using two WS2812-LEDs
 * - left    Color of left eye (rows)
 * - right   Color of left eye (columns)
 *********************************************/
void setEyes(RgbColor left, RgbColor right) {
  strip.SetPixelColor(0, left);
  strip.SetPixelColor(1, right);
  strip.Show();
}


/*********************************************
 * processEyes
 * - set the LED to next state
 * returns:
 * - time [ms] for this state to remain active
 *********************************************/
uint16_t processEyes(void) {  
  uint16_t pause = 1000;  
  if (g_cur_eyestate) {
    g_cur_eyestate = false;
    // IF Eyes are ON, switch them OFF now.
    setEyes(black, black);    
    pause = TAP_TIME;    
  } else {
    g_cur_eyestate = true;
    // IF Eyes are OFF, get net State ans switch LEDs    
    switch (nextEyeState()) {
      case 'Z':
        // setEyes(g_cur_color_z, black); // Tap ROW
        setEyes(g_cur_color_z, g_cur_color_z); // Tap ROW
        pause = TAP_TIME;        
        break;
      case 'S':
        // setEyes(black, g_cur_color_s); // Tap Column
        setEyes(g_cur_color_s, g_cur_color_s); // Tap Column
        pause = TAP_TIME;        
        break;
      case 'P':
        setEyes(black, black); // No Tap
        pause = CHAR_PAUSE_TIME;
        break;
      case 'E': // String completed: Change Coloer, restart        
        setEyes(black, black);
        next_color();
        pause = STARTOVER_TIME;
        break;
    }    
  }
  return(pause);
}

/************************************
 * Change Color of Morse LED
 * - every loop the color is changed
 ************************************/
void next_color(void) {  
  if (g_cur_color_z == lila1) {
    g_cur_color_z = green;
    g_cur_color_s = lila1;    
  } else {
    g_cur_color_z = lila1;
    g_cur_color_s = green;    
  }   
}

/*************************************************
 * nextEyeState is called every periode "Dit"
 * - returns: 
 *      "Z" if left  LED has to be switched on (Row)
 *      "S" if right LED has to be switched on (Column)
 *      "P" if both  LEDs have to be switched off
 *      "E" if both  finished sending the String
 * - Global Vars used    
 *   - g_cur_charpos to save the position of the current char
 *   - g_cur_bitpos  to save the position of the current morse-bit
 *   - g_cur_eyestate   to svae weather any LED is ON or OFF 
 *************************************************/
char nextEyeState(void) {
  char cur_char;         // eg. "H"
  byte cur_morse_idx;    // eg. "7" (in case of "H")
  char cur_tap;          // eg. "1"
  int retval;
  // get current char in Message
  cur_char = c_message [g_cur_charpos];
  // if not end of string
  if (cur_char != 0) {
    // transform to position in morse_table
    if ((cur_char >= 'A') && (cur_char <= 'Z')) {
      cur_morse_idx = cur_char - 'A';                // A-Z maps to 0-25
    } else {
      cur_morse_idx = 26;                            // Space maps to 26
    } 
    // get current Tapcode "Z", "S" or "P"
    cur_tap = tap_table[cur_morse_idx][g_cur_bitpos];
    // still bits left?
    if (cur_tap != 0) {
      retval = cur_tap;      
      g_cur_bitpos++;
    } else {                          // end of char tapcode, goto next char
      retval = 'P';
      g_cur_bitpos = 0;
      g_cur_charpos++;
    }
  } else {                            // last char in textBuffer
    g_cur_charpos = 0;
    g_cur_bitpos = 0;
    retval = 'E';
  }
  return (retval);
}




/************************
 * Process Serial Commands
 ************************/
void processSerial(void) {
  String cmd;
  String res;
  int n;
  cmd = "";
  cmd = Serial.readStringUntil('\n');   
  if (cmd.startsWith("wink ")) {
    res = cmd.substring(5, cmd.length());
    n = res.toInt();
    if (n < 0) {
      n = 0;
    } else if (n > 25) {
      n = 25;
    }
    wink((uint8_t) n);
    DBG_COMMAND.print("winking N: "); 
    DBG_COMMAND.println(n); 
  } else { 
    DBG_COMMAND.print("unknown Command: "); 
    DBG_COMMAND.println(cmd); 
  }
}


/************
 * Main Loop
 ************/
void loop() {  
  // next morsebit
  if ((millis() - g_last_tap_millis) > g_cur_eye_delay) {
    g_last_tap_millis = millis();
    g_cur_eye_delay = processEyes();
  }  
  // Heartbeat
  if (DEBUG_HEARTBEAT) {
    if ((millis() - g_last_heartbeat_millis) > HEARTBEAT_TIME) {
      g_last_heartbeat_millis = millis();
      DBG_HEARTBEAT.println("36c3-Winkekatze");      
    }
  }  
  // Serial Commands
  if(Serial.available()){
    processSerial();
  }
}



