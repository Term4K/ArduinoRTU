#define VERSION F("3.00 11-Jul-2023")
#define INCORRECT F("Incorrect Command (Type HELP for list of commands)")
#include <DS3231_Simple.h>
#include <SimpleDHT.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <Ethernet.h>
//#include <EthernetUdp.h>
#include <LiquidCrystal_I2C.h>

//define the input length as well as the LED Pins
#define MAX_INPUT_LENGTH 20
#define RED_LED 8
#define GREEN_LED 9
#define DHT22_PIN 2
#define NUM_SWITCHES 5
#define BRIGHTNESS 50

//defining all the tokens to be used
#define t_ON 2
#define t_OFF 3
#define t_BLINK 4
#define t_SET 8
#define t_TEMP 11
#define t_CURRENT 12
#define t_HISTORY 13
#define t_HIGH 14
#define t_LOW 15
#define t_RGB 16
#define t_ADD 17
#define t_RTC 18
#define t_READ 19
#define t_WRITE 20
#define t_RGBBLINK 21
#define t_VERSION 253
#define t_HELP 254
#define t_WORD 252
#define t_EOL 255

#define NUM_LEDS 4
#define DATA_PIN 5

byte deviceID = 0x02;

//table mapping all of the tokens using the first two letters and length
//used for ease of looking up tokens
byte lookupTable[] = {
  'O', 'N', 2, t_ON,
  'O', 'F', 3, t_OFF,
  'B', 'L', 5, t_BLINK,
  'S', 'E', 3, t_SET,
  'V', 'E', 7, t_VERSION,
  'H', 'E', 4, t_HELP,
  'T', 'E', 4, t_TEMP,
  'C', 'U', 7, t_CURRENT,
  'H', 'I', 7, t_HISTORY,
  'L', 'O', 3, t_LOW,
  'H', 'I', 4, t_HIGH,
  'R', 'G', 3, t_RGB,
  'A', 'D', 3, t_ADD,
  'R', 'T', 3, t_RTC,
  'R', 'E', 4, t_READ,
  'W', 'R', 5, t_WRITE,
  'R', 'G', 8, t_RGBBLINK
};

enum AlarmState {
  MAJOR_UNDER,
  MINOR_UNDER,
  COMFORTABLE,
  MINOR_OVER,
  MAJOR_OVER
};

enum ScreenState {
  MAIN,
  HOME,
  HISTORY,
  STATS,
  CHECKSUM_ERR,
  SETTINGS  
};

enum SettingState {
  SETTING_MAIN,
  IP_ADDRESS,
  SUBNET,
  GATEWAY,
  TEMPERATURE_SETTINGS,
  ERASE_HISTORY,
  EXIT
};

enum TempMenu {
  TEMP_MAIN,
  UNDER_MAJOR,
  UNDER_MINOR,
  OVER_MINOR,
  OVER_MAJOR
};

int debounceDelay = 100;
ScreenState menu = MAIN;
SettingState settings = SETTING_MAIN;
TempMenu tempSettings = TEMP_MAIN;
int prevSwitchState = 255;
int currSwitchState;
unsigned long lastDebounce = 0;
int errThresh = 100;
const int switchThreshold[NUM_SWITCHES] = {745, 505, 333, 147, 0};

const int numMenuItemsMain = 4;
int selectedMenuItem = 0;
int menuOffset = 0;
const char* mainMenu[numMenuItemsMain] = {
  "Home",
  "History",
  "Stats",
  "Settings"
};

const int numSettingsItems = 6;
int selectedSetting = 0;
int settingOffset;
const char* settingsMenu[numSettingsItems] = {
  "IP Address",
  "Subnet",
  "Gateway",
  "Temp Settings",
  "Erase History",
  "Exit to Main"
};

const int numTempItems = 5;
int selectedTemp = 0;
int tempOffset;
const char* tempMenu[numTempItems] = {
  "Major Under",
  "Minor Under",
  "Minor Over",
  "Major Over",
  "Exit to Settings"
};

byte cursorIndicator[8] = {
  0b00000,
  0b00000,
  0b01000,
  0b11111,
  0b01000,
  0b00000,
  0b00000,
  0b00000
};

byte changePointer[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00000,
  0b00000
};

struct sendData {
  byte address;
  float temperature;
};

//buffer to hold the tokens after the input is processed along with the index
byte tokenBuffer[8];
int tokenIndex = 0;

//the input buffer using the defined max length
char input[MAX_INPUT_LENGTH];
int inputIndex = 0;

unsigned int blinkIntervalRGB = 500;
unsigned long prevMillisDuo = 0; //same as inboard but different value in case of different activation times
unsigned long prevMillisRGB = 0;
//bools used to set the blinking as well as the state which also is used in the blinking function
bool blinkRGB = false;
bool rgbState = false;
bool etherCommand = false;
int written = -1;

AlarmState currentAlarm = COMFORTABLE;

SimpleDHT22 dht22(DHT22_PIN);
unsigned long prevMillisSensor = 0;
float temperature = 0;
float humidity = 0;

DS3231_Simple Clock;

CRGB leds[NUM_LEDS];
byte LED_RGB[3] = {255, 255, 255};

EthernetUDP Udp;
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
byte ipAdd[] = {EEPROM.read(0),EEPROM.read(1),EEPROM.read(2),EEPROM.read(3)};
byte subnet[] = {EEPROM.read(4),EEPROM.read(5),EEPROM.read(6),EEPROM.read(7)};
byte gateway[] = {EEPROM.read(8),EEPROM.read(9),EEPROM.read(10),EEPROM.read(11)};
IPAddress ip(ipAdd[0], ipAdd[1], ipAdd[2], ipAdd[3]);
IPAddress retIP(192, 168, 1, 1);
unsigned int recievedPackets = 0;
unsigned int sentPackets = 0;

unsigned int localPort = 8888;
byte packetBuffer[UDP_TX_PACKET_MAX_SIZE];

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup(){
 
  Serial.begin(9600);
  FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  Clock.begin();
  Ethernet.init(10);
  Ethernet.begin(mac, ip);
  
  Udp.begin(localPort);
  
  lcd.init();                      // initialize the lcd 
  lcd.backlight();
  lcd.createChar(0x7E, cursorIndicator);
  lcd.createChar(0, changePointer);
  updateMainMenu();
  
  //set all LED pins to output
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT); 
  leds[2].setRGB(0, 255, 0);
  FastLED.show();
}

void loop(){
  //blink the LED constantly if the LED is set to blink (does not interfere with input)
  blinkLED();
  saveTempAndHumidity();
  setEthStatusLED();
  updateAlarm();
  parseUDPCommands();
  switchInput();
}

void(* resetFunc) (void) = 0;

//blink the LED using the system time to make sure timing between blinks is right
void blinkLED(){
  unsigned long currMillis = millis();

  if(blinkRGB){
    if(currMillis - prevMillisRGB >= blinkIntervalRGB){
      prevMillisRGB = currMillis;
      rgbState = !rgbState;
      switchRGB();
    }
  }
}


