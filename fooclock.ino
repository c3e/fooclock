#include <Time.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <TimerOne.h>


byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// NTP Servers:
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov


const int timeZone = 1;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

int cur_sec         = 0;
int cur_min         = 0;
int cur_update      = 0;
int shift_state     = 0;
int update_counter  = 0;

EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

int OutputEnable    = 3;

//Pin connected to ST_CP of 74HC595
int latchPin        = 4;

//Pin connected to SH_CP of 74HC595
int clockPin        = 2;

//Pin connected to DS of 74HC595
int dataPin         = 5;

//                4
//            +/=====/+
//           //     //
//       8  //     // 2
//         //  16 //
//        +/=====/+
//       //     //
//  128 //     // 32
//     //  64 //
//    +/=====/+ O  1

//          4
//      /=====\
//     ||     ||
//   8 ||     ||  2
//     ||  16 ||
//      >=====<
//     ||     ||
// 128 ||     || 32
//     ||  64 ||
//      \=====/ O  1


int frame [6] = {
    0,0,0,0,0,0
};
int today [6] = {
    0,0,0,0,0,0
};


int offset = 0;

int digits[10] = {
  238, //0 0xEE
  34,  //1 0x22
  214, //2 0xD6
  118, //3 0x76
  58,  //4 0x3A
  124, //5 0x7C
  252, //6 0xFC
  38,  //7 0x26
  254, //8 0xFE
  126  //9 0x7E
};

void setup()
{
  pinMode (OutputEnable, OUTPUT);
  pinMode (latchPin,     OUTPUT);
  pinMode (clockPin,     OUTPUT);
  pinMode (dataPin,      OUTPUT);

  Serial.begin(9600);

  while (Ethernet.begin(mac) == 0) {
    delay(5000);
  }
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);

  Timer1.initialize(10000);  // initialize timer1, and set a 10 milli second period
  Timer1.attachInterrupt(updateDisplay);  // attaches callback() as a timer overflow interrupt

  analogWrite(OutputEnable,125);
}

void loop()
{
  time_t t = now();
  //int test [] = {184,220,200,200,240,0}; // Display "Hello"
  if (second(t) >= offset && second(t) <= offset+10 && minute(t) != cur_min )
  {
    transition(today);
    //displayDate(t);
    if (second(t)==offset+10){
        displayDate_(t);
        offset+=10;
        cur_min = minute(t);
        shift_state = 0;
    }
  }
  else
  displayTime(t);

  if (offset == 50){
    offset = 0;
  }
}

void displayDate_(time_t t)
{
  today[0] = digits[(day(t)         /10)];
  today[1] = digits[(day(t)         %10)];

  today[2] = digits[(month(t)       /10)];
  today[3] = digits[(month(t)       %10)];

  today[4] = digits[((year(t)-2000) /10)];
  today[5] = digits[((year(t)-2000) %10)];
}

void shift_right(int neu)
{
    for ( int i= 0; i< 5; i++){
      frame[i] = frame[i+1];
    }
    frame[5] = neu;
}

void transition(int* a){
  time_t t = now();
  if (update_counter % 30 == 0 && cur_update != update_counter && shift_state < 6){
    cur_sec     = second(t);
    cur_update  = update_counter;
    shift_right(a[5-shift_state]);
    shift_state ++;
  }
}

void updateDisplay ()
{
  // take the latchPin low so
  // the LEDs don't flicker while you're sending in bits:
  digitalWrite(latchPin, LOW);

  for(int digitCount=5; digitCount>=0; digitCount--)
  {
    shiftOut(dataPin, clockPin, MSBFIRST, frame[digitCount]);
  }
  //take the latch pin high so the LEDs will light up:
  digitalWrite(latchPin, HIGH);

  update_counter++;

  if(update_counter == 255){
    update_counter = 0;
  }
}

void displayTime(time_t t)
{
  frame[5] = digits[(hour(t)         /10)];
  frame[4] = digits[(hour(t)         %10)];

  frame[3] = digits[(minute(t)       /10)];
  frame[2] = digits[(minute(t)       %10)];

  frame[1] = digits[(second(t)       /10)];
  frame[0] = digits[(second(t)       %10)];
}


void displayDate(time_t t)
{
  frame[5] = digits[(day(t)          /10 )];
  frame[4] = digits[(day(t)          %10 )];

  frame[3] = digits[(month(t)        /10 )];
  frame[2] = digits[(month(t)        %10 )];

  frame[1] = digits[((year(t)-2000)  /10 )];
  frame[0] = digits[((year(t)-2000)  %10 )];
}

void displayBinaryTime(time_t t)
{
  int digit;

  for(int digitCount=5; digitCount>=0; digitCount--)
  {
    //digit zusammenbauen und dann invertieren
    digit = 0B00000000;
    bitWrite(digit,2,(bitRead(hour(t),    digitCount)) );
    bitWrite(digit,4,(bitRead(minute(t),  digitCount)) );
    bitWrite(digit,6,(bitRead(second(t),  digitCount)) );
    frame[digitCount]= digit;
  }
}



/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0]   = 0b11100011;   // LI, Version, Mode
  packetBuffer[1]   = 0;     // Stratum, or type of clock
  packetBuffer[2]   = 6;     // Polling Interval
  packetBuffer[3]   = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

