#include <avr/pgmspace.h>
#include "libraries/MatrixDisplay/MatrixDisplay.h"
#include "libraries/MatrixDisplay/DisplayToolbox.h"
#include "libraries/MatrixDisplay/font.h"

#include "libraries/ethercard/EtherCard.h"
// configure buffer size to 700 octets
uint8_t Ethernet::buffer[700];
static const uint8_t mymac[] = { 0x36, 0xA9, 0x34, 0x4A, 0x61, 0xF4 };

// Easy to use function
#define setMaster(dispNum, CSPin) initDisplay(dispNum,CSPin,true)
#define setSlave(dispNum, CSPin) initDisplay(dispNum,CSPin,false)

#define SIGN_NUM_DISPLAYS 4
#define SIGN_WR_PIN 10
#define SIGN_DATA_PIN 9
#define SIGN_SHADOW_BUF true

// Init Matrix
MatrixDisplay disp(SIGN_NUM_DISPLAYS,
                   SIGN_WR_PIN,
                   SIGN_DATA_PIN,
                   SIGN_SHADOW_BUF);
// Pass a copy of the display into the toolbox
DisplayToolbox toolbox(&disp);
static const char PROGMEM charLookup[]  = " 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*(),-.?></\\|[]_=+:'\"{}";
static const char PROGMEM httpResponse[] = "HTTP/1.1 204 No Content\r\n";


// Prepare boundaries
uint8_t X_MAX = 0;
uint8_t Y_MAX = 0;

// Memory test
extern int __bss_end;
extern int *__brkval;

// Message holder
char message[256];

void setup() {
  // Fetch bounds
  X_MAX = disp.getDisplayCount() * disp.getDisplayWidth();
  Y_MAX = disp.getDisplayHeight();

  // Prepare displays
  // The first number represents how the buffer/display is stored in memory. Could be useful for reorganising the displays or matching the physical layout
  // The number is a array index and is sequential from 0. You can't use 4-8. You must use the numbers 0-4
  // The second number represents the "CS" pin (ie: CS1, CS2, CS3, CS4) this controls which panel is enabled at any one time.
  disp.setMaster(0,4);
  disp.setSlave(1,5);
  disp.setSlave(2,6);
  disp.setSlave(3,7);

  // Briefly light the entire panel to show any non-working LEDs
  flash(2000);

  initNetwork();
}
//
void loop()
{
  unsigned long pos;
  pos = ether.packetLoop(ether.packetReceive());// check if valid tcp data is received
  if (pos) {
    char* data = (char *) Ethernet::buffer + pos;
    if (strncmp("POST /message", data, 13) == 0) {
      char *param = strstr(data, "msg=");
      if (param != 0) {
        param += 4;
        snprintf(message, sizeof message - 1, "%s", param);
      } else {
        snprintf_P(message, sizeof message - 1, PSTR("Received POST with no data!"));
      }

      ether.httpServerReplyAck();
      memcpy_P(ether.tcpOffset(), httpResponse, sizeof httpResponse);
      ether.httpServerReply_with_flags(sizeof httpResponse - 1, TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);
      scrollText(message, true);
    }
  } else {
    snprintf_P(message, sizeof message - 1, PSTR("Free Memory: %d"), get_free_memory());
    fixedText(message);
  }
}

// Initialize network card
void initNetwork() {
  // define (unique on LAN) hardware (MAC) address

  uint8_t nFirmwareVersion = ether.begin(sizeof Ethernet::buffer, mymac);
  if (0 == nFirmwareVersion) {
    //snprintf_P(message, sizeof(message)-1, PSTR("Error initializing ethernet card!"));
    //scrollText(message, true);
    //flash(2000);
    return;
  }

  if (!ether.dhcpSetup()) {
    snprintf_P(message, sizeof(message)-1, PSTR("DHCP Failed!"));
    scrollText(message, true);
    return;
  }
  snprintf_P(message, sizeof(message)-1, PSTR("Net v%d OK. IP: %d.%d.%d.%d"), nFirmwareVersion, ether.myip[0], ether.myip[1], ether.myip[2], ether.myip[3]);

  scrollText(message, true);
}

void scrollText(char* text, int pastEnd) {
  int y=1;
  int endPos = 0;
  if(pastEnd)
    endPos =  -(strlen(text) * 7);

  for(int Xpos = X_MAX; Xpos > endPos; Xpos--){
    disp.clear();
    drawString(Xpos,y,text);
    disp.syncDisplays();
    delay(1);
  }

//  EXPERIMENTAL IMPLEMENTATION! NOT WORKING!
//  disp.clear();
//  drawString(X_MAX-100,y,text);
//  disp.syncDisplays();
//  int pos = 0;
//  while (pos > endPos) {
//    disp.shiftLeft();
//    disp.syncDisplays();
//    pos--;
//    delay(5);
//  }
  delay(50);
}

// Write a line of static (non-scrolling) text to the display
void fixedText(char* text){
  int y = 1;
  int x = 0;
  disp.clear();
  drawString(x,y,text);
  disp.syncDisplays();
}


// Output a string to the display
void drawString(int x, uint8_t y, char* c){
  for(char i=0; i< strlen(c); i++){
    drawChar(x, y, c[i]);
    x+=6; // Width of each glyph
  }
}

void flash(int duration) {
  for(int y=0; y < Y_MAX; ++y)	{
    for(int x = 0; x< X_MAX; ++x) {
      toolbox.setPixel(x, y, 1, true); // Lets write straight to the display.
    }
  }
  delay(duration);
  disp.clear();
}


// Output a single character to the display
void drawChar(int x, int y, char c){
  int dots;

  const char *offset = strchr_P(charLookup, c);
  int idx = offset - charLookup;

  for (char col=0; col< 5; col++) {
    if((x+col+1)>0 && x < X_MAX){ // dont write to the display buffer if the location is out of range
      dots = pgm_read_byte_near(&myfont[idx][col]);
      for (char row=0; row < 7; row++) {
        if (dots & (64>>row))   	     // only 7 rows.
          toolbox.setPixel(x+col, y+row, 1);
        else
          toolbox.setPixel(x+col, y+row, 0);
      }
    }
  }
}

// Memory check
int get_free_memory(){
  int free_memory;
  if((int)__brkval == 0)
    free_memory = ((int)&free_memory) - ((int)&__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int)__brkval);
  return free_memory;
}
