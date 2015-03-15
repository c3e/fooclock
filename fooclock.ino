#include <Time.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <TimerOne.h>

#define A 0
#define B 1
#define C 2
#define D 3
#define E 4
#define F 5
#define G 6
#define H 7
#define I 8
#define J 9
#define K 10
#define L 11
#define M 12
#define N 13
#define O 14
#define P 15
#define Q 16
#define R 17
#define S 18
#define T 19
#define U 20
#define V 21
#define W 22
#define X 23
#define Y 24
#define Z 25

#define UP		1
#define DOWN	2

// ---------------------------------- Definition of Global Variables -------------------------- //

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

time_t cur_time     = 0;
time_t alarm_goal	= 0;
bool update_done    = false;
bool second_changed = false;
bool init_done		= false;
int cur_update      = 0;
int shift_state     = 0;
int update_counter	= 0;
int spinner_pos		= 0;

EthernetUDP Udp;
EthernetUDP AlarmClock;
unsigned int alarmClockPort = 123; // local port to listen for custom AlarmClock packets
unsigned int localPort		= 8888;  // local port to listen for UDP packets

int OutputEnable    = 3;

//Pin connected to ST_CP of 74HC595
int latchPin        = 4;

//Pin connected to SH_CP of 74HC595
int clockPin        = 2;

//Pin connected to DS of 74HC595
int dataPin         = 5;

//          4
//      /======\
//     ||      ||
//   8 ||      ||  2
//     ||  16  ||
//      >======<
//     ||      ||
// 128 ||      || 32
//     ||  64  ||
//      \======/ O  1

int duration = 10;

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

int letter [26] = {

	191, // A  
	248, // b
	204, // C
	242, // d 
	220, // E  
	156, // F  
	126, // g 
	184, // h 
	32,  // i  
	230, // J  
	190, // k 
	200, // L 
	176, // m  
	176, // n  
	240, // o  
	158, // P  
	238, // Q  
	144, // r  
	124, // S  
	216, // t  
	224, // u  
	224, // v  
	26,  // w  
	186, // X 
	58,  // y 
	214, // Z
};

int frame [6] = {
	0,letter[T],letter[R],letter[A],letter[T],letter[S] // Display "Start" (write from right to left into the array)
};
int today [6] = {
	184,220,200,200,240,0 // Display "Hello"
};

// ----------------------------------- Setup Code ---------------------------------------------- //
void setup()
{
	pinMode (OutputEnable, OUTPUT);
	pinMode (latchPin,     OUTPUT);
	pinMode (clockPin,     OUTPUT);
	pinMode (dataPin,      OUTPUT);

	analogWrite(OutputEnable,125);

	Timer1.initialize(1000);  // initialize timer1, and set a 1 milli second period
	Timer1.attachInterrupt(updateDisplay);  // attaches callback() as a timer overflow interrupt
	updateDisplay();

	Serial.begin(9600);

	while (Ethernet.begin(mac) == 0) {
		delay(5000);
	}
	Udp.begin(localPort);
	setSyncProvider(getNtpTime);

	update_counter = 0;
	init_done = true;
}

// -------------------------------------- Main Loop ------------------------------------------------- //
void loop()
{
	time_t timestamp = now();
	//Sync blinking with second change
	if(cur_time != timestamp && !second_changed){
		update_counter = 0;
		cur_time = timestamp;
		second_changed = true;
	}

	int intervalpos = timestamp%70;
	if( intervalpos == 0){
		update_done = false;
	}
	if (intervalpos < 10){
		transition(today);
		//displayDate(timestamp);
	}else if (intervalpos > 20 && intervalpos < 30){
		displayBinaryTime(timestamp);
	}else{
		displayTime(timestamp);
	} 

	if(intervalpos == 50 && !update_done){
		update_done = true;
		updateDate(timestamp);
		shift_state = 0;
	}

	// Blinkenfoo - comment to remove pulsating light
	if(update_counter < 125  && timestamp%600 == 0 ){
		dim(UP,125,255);
	}
	if(update_counter > 875  && timestamp%600 == 0 ){
		dim(DOWN,125,255);
	}
	if(update_counter == 900){
		second_changed = true;
	}


}

// ---------------------- Interrupt Handler (Timer1) ------------------- //
void updateDisplay ()
{
	// take the latchPin low so
	// the LEDs don't flicker while you're sending in bits:
	digitalWrite(latchPin, LOW);

	for(int digitCount=5; digitCount>=0; digitCount--)
	{
		shiftOut(dataPin, clockPin, MSBFIRST, frame[digitCount]);
	}
	//take the latch pin high so the LEDs will light up again:
	digitalWrite(latchPin, HIGH);

	update_counter++;

	if(update_counter == 1000){
		update_counter = 0;
	}

	if(!init_done){
		if(update_counter % 100 == 0){
			spin(0);
		}
	}
}

// -------------------------------- Date Functions ---------------------- //
void updateDate(time_t t)
{
	today[0] = digits[(day(t)         /10)];
	today[1] = digits[(day(t)         %10)];

	today[2] = digits[(month(t)       /10)];
	today[3] = digits[(month(t)       %10)];

	today[4] = digits[((year(t)-2000) /10)];
	today[5] = digits[((year(t)-2000) %10)];
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

// ------------------------------- Time Functions -------------------- //
void displayTime(time_t t)
{
	frame[5] = digits[(hour(t)         /10)];
	frame[4] = digits[(hour(t)         %10)];

	frame[3] = digits[(minute(t)       /10)];
	frame[2] = digits[(minute(t)       %10)];

	frame[1] = digits[(second(t)       /10)];
	frame[0] = digits[(second(t)       %10)];
}

void displayBinaryTime(time_t t)
{
	int digit;

	for(int digitCount=5; digitCount>=0; digitCount--)
	{
		//digit zusammenbauen und dann invertieren
		digit = 0;

		digit |= ((hour(t)   & (1 << digitCount))>>digitCount)<<2;
		digit |= ((minute(t) & (1 << digitCount))>>digitCount)<<4;
		digit |= ((second(t) & (1 << digitCount))>>digitCount)<<6;

		//bitWrite(digit,2,(bitRead(hour(t),    digitCount)) );
		//bitWrite(digit,4,(bitRead(minute(t),  digitCount)) );
		//bitWrite(digit,6,(bitRead(second(t),  digitCount)) );
		frame[digitCount]= digit;
	}
}


// ----------------------------------- System Functions ----------------------------------- //
void dim(int direction, int lower_limit, int upper_limit){
	int range = upper_limit - lower_limit;
	if(direction == DOWN && update_counter % range <= range ){
		analogWrite(OutputEnable, lower_limit + (update_counter % range));
		Serial.println("Down ->");
		Serial.println(lower_limit+update_counter % range);
	} else{
		analogWrite(OutputEnable, upper_limit - (update_counter % range));
		Serial.println("UP ->");
		Serial.println(upper_limit - update_counter % range);
	}
}

void spin(int digit){
	if(spinner_pos >= 6)spinner_pos = 0;
	switch (spinner_pos){
		case  0: frame[digit] =   4; break;
		case  1: frame[digit] =   2; break;
		case  2: frame[digit] =  32; break;
		case  3: frame[digit] =  64; break;
		case  4: frame[digit] = 128; break;
		case  5: frame[digit] =   8; break;
		default: frame[digit] =   0; break;
	}
	spinner_pos ++;
};

void shift_right(int neu)
{
	for ( int i= 0; i< 5; i++){
		frame[i] = frame[i+1];
	}
	frame[5] = neu;
}

void transition(int* a){
	time_t t = now();
	if (update_counter % 300 == 0 && cur_update != update_counter && shift_state < sizeof(today)/2){		//assuming the Array contains only ints, 
																										//which have a sizeof 2 on this Arduino
		Serial.println(sizeof(a));
		cur_update  = update_counter;
		shift_right(a[5-shift_state]);
		shift_state ++;
	}
}






/*-------- Alarm Clock code ---------*/
const int AlarmClockPacketSize = 256;
byte AlarmBuffer[AlarmClockPacketSize];

int listenForAlarm(){
	while (AlarmClock.parsePacket() > 0);
	Serial.println("Any Alarm imminent?");
	uint32_t packageBegin = millis();
	while(millis() - packageBegin < 1500){
		int size = AlarmClock.parsePacket();
		if(size >= AlarmClockPacketSize){
			Serial.println("Alarm seems to be imminent.");
			AlarmClock.read(AlarmBuffer, AlarmClockPacketSize);
			alarm_goal = (unsigned long)AlarmBuffer;
			return 1;
		}
	}
	Serial.println("No Alarm present!");
	return 0;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
	while (Udp.parsePacket() > 0) ; // discard any previously received packets
	//Serial.println("Transmit NTP Request");
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
	//Serial.println("No NTP Response :-(");
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

