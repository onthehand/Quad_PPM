#include <TimerOne.h>
#include <Wii.h>
#include <usbhub.h>

#define CHANNELS 6

#define ROLL     0
#define PITCH    1
#define THROTTLE 2
#define YAW      3
#define AUX1     4
#define AUX2     5

// Set these a little high since they are registering low on AQ
#define MIN_PULSE_TIME  1000 // 1000us
#define MAX_PULSE_TIME  2000 // 2000us
#define HALF_PULSE_TIME  (MIN_PULSE_TIME + MAX_PULSE_TIME) / 2
#define LOW_PULSE_TIME  (MIN_PULSE_TIME * 3 + MAX_PULSE_TIME) / 4
#define HIGH_PULSE_TIME  (MIN_PULSE_TIME + MAX_PULSE_TIME * 3 ) / 4
#define HALF_TH 1350
#define SYNC_PULSE_TIME  3050 // 3000us
#define SPAN 10

#define PIN_PPM 5
#define PIN_CAMERA 2

unsigned int pw[CHANNELS];

unsigned int currentTh = HALF_TH;
unsigned long uptime = 0;
unsigned int activate = 0;

USB Usb;
USBHub Hub1(&Usb);
BTD Btd(&Usb);
WII Wii(&Btd,PAIR);

void setPulse(unsigned int r, unsigned int p, unsigned int t, unsigned int y, unsigned int a1, unsigned int a2){
  pw[ROLL] = r;
  pw[PITCH] = p;
  pw[THROTTLE] = t;
  pw[YAW] = y;
  pw[AUX1] = a1;
  pw[AUX2] = a2;
}

unsigned int checkButton(Button b1, Button b2, unsigned int high, unsigned int half, unsigned int low){
  if( Wii.getButtonPress(b1) ){
    return high;
  }else if( Wii.getButtonPress(b2) ){
    return low;
  }
  return half;
}

void setup() {
  if (Usb.Init() == -1) { while(1); }

  pinMode(PIN_PPM, OUTPUT);
  pinMode(PIN_CAMERA, OUTPUT);
  digitalWrite( PIN_CAMERA, LOW );

  Serial.begin(38400);
 
  activate = 0;
  
  // Start timer with sync pulse
  Timer1.initialize(SYNC_PULSE_TIME);
  Timer1.attachInterrupt(isr_sendPulses);
  isr_sendPulses();
}

float pp = 0.0;
float pr = 0.0;
unsigned int cc = 0;
#define WII_TIMEOUT 2000

unsigned long camtrig = 0;
#define CAMERA_PULSE 1500

void loop() {
  Usb.Task();
  if(! Wii.wiimoteConnected || Wii.getButtonPress(HOME)){ activate = 0; return; }
  if( millis() > uptime ){
    // check connecition
    if( pp == Wii.getPitch() && pr == Wii.getRoll() ){ cc ++ ; }else{ cc = 0; }
    pp = Wii.getPitch();
    pr = Wii.getRoll();

    if(Wii.getButtonPress(RIGHT)){
      currentTh++;
    }else if(Wii.getButtonPress(LEFT)){
      currentTh--;
    }
    uptime = millis() + SPAN;    
  }
  if ( cc > WII_TIMEOUT / SPAN ){ activate = 0; return; }
  activate = 1;
  
  if( 0 < camtrig && camtrig < millis() ){
    camtrig = 0;
    digitalWrite( PIN_CAMERA, LOW );
  }
  if( camtrig == 0 && Wii.getButtonPress(TWO) ){
    camtrig = millis() + CAMERA_PULSE;
    digitalWrite( PIN_CAMERA, HIGH );
  }


  unsigned int y = checkButton(DOWN,UP,HIGH_PULSE_TIME,HALF_PULSE_TIME,LOW_PULSE_TIME);
  unsigned int r = map((unsigned int)(Wii.getPitch()*100.0), 9000, 27000, MAX_PULSE_TIME, MIN_PULSE_TIME);
  unsigned int p = map((unsigned int)(Wii.getRoll()*100.0), 9000, 27000, MIN_PULSE_TIME, MAX_PULSE_TIME);

  // BARO is supposed to be activated by AUX1.
  unsigned int a1 = (Wii.getButtonPress(ONE)) ? HIGH_PULSE_TIME : HALF_PULSE_TIME;
  unsigned int a2 = (Wii.getButtonPress(TWO)) ? HIGH_PULSE_TIME : HALF_PULSE_TIME;
  if( Wii.getWiiState() & 0x01 ){ a2 = LOW_PULSE_TIME; }
  if(Wii.getButtonPress(B)){
    r = p = HALF_PULSE_TIME;
    if(Wii.getButtonPress(PLUS)){
      // arm
      currentTh = 1000;
      y = 2000;
    }else if(Wii.getButtonPress(MINUS)){
      // disarm
      currentTh = y = 1000;
    }
  }

  setPulse(r,p,currentTh,y,a1,a2);
}

// Sync pulse first
volatile int currentChannel = 0;

void isr_sendPulses() {
  digitalWrite(PIN_PPM, LOW);
  if( ! activate ){ return; }

  if (currentChannel == CHANNELS) {
    // After last channel
    Timer1.setPeriod(SYNC_PULSE_TIME);
    currentChannel = 0; // Will be 0 on next interrupt
  } else {
    if(pw[currentChannel] < MIN_PULSE_TIME){ pw[currentChannel] = MIN_PULSE_TIME; }
    if(pw[currentChannel] > MAX_PULSE_TIME){ pw[currentChannel] = MAX_PULSE_TIME; }
    Timer1.setPeriod(pw[currentChannel]);
    currentChannel++;
  }

  digitalWrite(PIN_PPM, HIGH);
}

