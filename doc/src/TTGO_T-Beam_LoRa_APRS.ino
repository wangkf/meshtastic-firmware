#include <Arduino.h>
#include <SPI.h>
#include <BG_RF95.h>         // library from OE1ACM
#include <TinyGPS++.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <axp20x.h>
// Configuration constants as String objects
const String CALLSIGN = "BI9ABS-2";     // enter your callsign here - less then 6 letter callsigns please add "spaces" so total length is 6 (without SSID)
const String LONGITUDE_PRESET = "10852.36E"; // please in APRS notation DDDMM.mmE or DDDMM.mmW
const String LATITUDE_PRESET = "3414.97N";   // please in APRS notation DDMM.mmN or DDMM.mmS
const String APRS_SYMBOL = "b";         // other symbols are
                                // "_" => Weather Station
                                // ">" => CAR
                                // "[" => RUNNER
                                // "b" => BICYCLE
                                // "<" => MOTORCYCLE
                                // "R" => Recreation Vehicle
const String MY_COMMENT = "Unit Test"; // add your coment here - if empty then no comment is sent
// TRANSMIT INTERVAL
unsigned long max_time_to_nextTX = 180000L;   // set here MAXIMUM time in ms(!) for smart beaconing - minimum time is always 1 min = 60 secs = 60000L !!!
                                // when entering 60000L intervall is fixed to 1 min
// TTGO T-Beam pin definitions
#define SSD1306_ADDRESS 0x3C
static const uint32_t GPSBaud = 9600; //GPS

#define I2C_SDA 21
#define I2C_SCL 22
#define OLED_RST 16
static const int RXPin = 12, TXPin = 34;  // GPS 串口
#define LORA_RESET 23                     // LoRa 复位引脚
#define LORA_NSS 18                       // LoRa NSS引脚
#define BATTERY_PIN 35                    // 电池电压检测
#define GPS_POWER 4                       // GPS 电源控制
// Version-specific pin definitions
#if (defined(T_BEAM_V1_0) || defined(T_BEAM_V1_1)|| defined(T_BEAM_V1_2)|| defined(T_BEAM_V2_0))
  #define TXLED 4                           // 发送指示灯
  #define BUTTON 38                         // 按钮
#else
  #warning "未指定TTGO T-Beam版本，使用默认V0.7引脚定义"
  #define TXLED 14                          // 发送指示灯
  #define BUTTON 39                         // 按钮
#endif
// Include OneButton library
#include "button_utils.h"
// Variables for APRS packaging
String sTable="/";           //Primer
// Tracker setting: use these lines to modify the tracker behaviour
#define TXFREQ  433.775      // Transmit frequency in MHz
#define TXdbmW  18           // Transmit power in dBm
// Variables and Constants
String Outputstring = "";
String outString="";
String LongShown="";
String LatShown="";
//byte arrays
byte  lora_TXBUFF[128];      //buffer for packet to send
//byte Variables
byte  lora_TXStart;          //start of packet data in TXbuff
byte  lora_TXEnd;            //end of packet data in TXbuff
unsigned long lastTX = 0L;
float BattVolts;
// variables for smart beaconing
float average_speed[5] = {0,0,0,0,0}, average_speed_final=0, max_speed=30, min_speed=0;
float old_course = 0, new_course = 0;
int point_avg_speed = 0, point_avg_course = 0;
ulong min_time_to_nextTX=60000L;      // minimum time period between TX = 60000ms = 60secs = 1min
ulong nextTX=60000L;          // preset time period between TX = 60000ms = 60secs = 1min
#define ANGLE 60              // angle to send packet at smart beaconing
#define ANGLE_AVGS 3          // angle averaging - x times
float average_course[ANGLE_AVGS];
float avg_c_y, avg_c_x;
#ifdef DEBUG
  // debug Variables
  String TxRoot="0";
  float millis_angle[ANGLE_AVGS];
#endif
static void smartDelay(unsigned long);
void recalcGPS(void);
void sendpacket(bool isTest = false);
void loraSend(byte, byte, byte, byte, byte, long, byte, float);
void batt_read(void);
void writedisplaytext(String, String, String, String, String, String, int);
//SoftwareSerial ss(RXPin, TXPin);   // The serial connection to the GPS device
HardwareSerial ss(1);        // TTGO has HW serial
TinyGPSPlus gps;             // The TinyGPS++ object
AXP20X_Class axp;
// Singleton instance of the radio driver
BG_RF95 rf95(LORA_NSS, 26);  // 使用当前定义的引脚
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST);// initialize OLED display
// is being "fed".
static void smartDelay(unsigned long ms){
  unsigned long start = millis();
  do
  {
      while (ss.available())
       gps.encode(ss.read());
  } while (millis() - start < ms);
}
char *ax25_base91enc(char *s, uint8_t n, uint32_t v){
  /* Creates a Base-91 representation of the value in v in the string */
  /* pointed to by s, n-characters long. String length should be n+1. */

  for(s += n, *s = '\0'; n; n--)
  {
    *(--s) = v % 91 + 33;
    v /= 91;
  }

  return(s);
}
//@APA Recalc GPS Position == generate APRS string
void recalcGPS(){
  String Ns, Ew, helper;
  char helper_base91[] = {"0000\0"};
  float Tlat=48.2012, Tlon=15.6361;
  int i, Talt, lenalt;
  uint32_t aprs_lat, aprs_lon;
  float Lat=0.0;
  float Lon=0.0;
  float Tspeed=0, Tcourse=0;
  String Speedx, Coursex, Altx;

    Tlat=gps.location.lat();
    Tlon=gps.location.lng();
    Talt=gps.altitude.meters() * 3.28;
    Altx = Talt;
    lenalt = Altx.length();
    Altx = "";
    for (i = 0; i < (6-lenalt); i++) {
      Altx += "0";
    }
    Altx += Talt;
    Tcourse=gps.course.deg();
    Tspeed=gps.speed.knots();

    aprs_lat = 900000000 - Tlat * 10000000;
    aprs_lat = aprs_lat / 26 - aprs_lat / 2710 + aprs_lat / 15384615;
    aprs_lon = 900000000 + Tlon * 10000000 / 2;
    aprs_lon = aprs_lon / 26 - aprs_lon / 2710 + aprs_lon / 15384615;

    if(Tlat<0) { Ns = "S"; } else { Ns = "N"; }
    if(Tlat < 0) { Tlat= -Tlat; }
    unsigned int Deg_Lat = Tlat;
    Lat = 100*(Deg_Lat) + (Tlat - Deg_Lat)*60;

    if(Tlon<0) { Ew = "W"; } else { Ew = "E"; }
    if(Tlon < 0) { Tlon= -Tlon; }
    unsigned int Deg_Lon = Tlon;
    Lon = 100*(Deg_Lon) + (Tlon - Deg_Lon)*60;
    outString = "";
      // Use CALLSIGN directly as it's already a String
      for (i=0; i<CALLSIGN.length();++i){  // remove unneeded "spaces" from callsign field
        if (CALLSIGN.charAt(i) != ' ') {
          outString += CALLSIGN.charAt(i);
        }
      }
      outString += ">APRS,WIDE1-1,qAR:!";

      if(Tlat<10) {outString += "0"; }
      outString += String(Lat,2);
      outString += Ns;
      outString += sTable;
      if(Tlon<100) {outString += "0"; }
      if(Tlon<10) {outString += "0"; }
      outString += String(Lon,2);
      outString += Ew;
      outString += APRS_SYMBOL;
      if(Tcourse<100) {outString += "0"; }
      if(Tcourse<10) {outString += "0"; }
      Coursex = String(Tcourse,0);
      Coursex.replace(" ","");
      outString += Coursex;
      outString += "/";
      if(Tspeed<100) {outString += "0"; }
      if(Tspeed<10) {outString += "0"; }
      Speedx = String(Tspeed,0);
      Speedx.replace(" ","");
      outString += Speedx;
      outString += "/A=";
      outString += Altx;
      outString += " Batt=";
      outString += String(BattVolts,2);
      outString += ("V");
      outString += MY_COMMENT;
      #ifdef DEBUG
        outString += (" Debug: ");
        outString += TxRoot;
      #endif
    Serial.print("outString=");
    Serial.println(outString);
}
void blinker(int counter) {
  for (int i = 0; i < (counter-1); i++) {
    digitalWrite(TXLED, HIGH);  // turn blue LED ON
    smartDelay(150);
    digitalWrite(TXLED, LOW);  // turn blue LED OFF
    smartDelay(100);
  }
  digitalWrite(TXLED, HIGH);  // turn blue LED ON
  smartDelay(150);
  digitalWrite(TXLED, LOW);  // turn blue LED OFF
}
// Generate APRS packet using preset location
void sendpacketWithPresetLocation(bool isTest = false) {
  String comment = isTest ? "/A=000000 [TEST]" : "/A=000000";
  // Use preset location from config file
  String LatAPRS = LATITUDE_PRESET;
  String LonAPRS = LONGITUDE_PRESET;
  // Add battery voltage information
  comment += " Batt=" + String(BattVolts,2) + "V";
  // Add custom comment if available
  if (MY_COMMENT.length() > 0) {
    comment += " " + MY_COMMENT;
    comment += " [PRESET]";
  }
  // Build APRS packet with correct APRS format
  Outputstring = CALLSIGN + ">APRS,WIDE1-1:!" + LatAPRS + sTable + APRS_SYMBOL + comment;
  Serial.print("outString[PRESET]=");
  Serial.println(Outputstring);
  // Send preset location packet
  memset(lora_TXBUFF, 0, sizeof(lora_TXBUFF));
  Outputstring.getBytes(lora_TXBUFF, Outputstring.length() + 1);
  Serial.print(String("[PACKET] ") + (isTest ? "TEST " : "") + "Preset location packet...");
  rf95.setModeTx();
  rf95.sendAPRS(lora_TXBUFF, Outputstring.length());
  rf95.waitPacketSent();
  rf95.setModeIdle();
  Serial.println("sent");
}
void sendpacket(bool isTest) {
  batt_read();
  Outputstring = "";

  if (gps.location.isValid() || gps.location.isUpdated()) {
    recalcGPS();
    Outputstring = outString;
    if (isTest) Outputstring += " TEST";  
    Serial.print(String("[PACKET] Using GPS data") + (isTest ? " (TEST)" : "") + "\n");
    
    if (!Outputstring.isEmpty()) {
      Serial.println("[PACKET] Ready to send");
      loraSend(lora_TXStart, lora_TXEnd, 60, 255, 1, 10, TXdbmW, TXFREQ);
    } else {
      Serial.println("[ERROR] Empty packet!");
    }
  } else {
    Serial.print(String("[PACKET] Using preset location") + (isTest ? " (TEST)" : "") + "\n");
    sendpacketWithPresetLocation(isTest);
  }
}
void loraSend(byte, byte, byte, byte, byte, long, byte power, float freq) {
  lastTX = millis();
  
  // Configure LoRa parameters
  rf95.setFrequency(freq);
  rf95.setModemConfig(BG_RF95::Bw125Cr45Sf4096);
  rf95.setTxPower(power);
  
  // Prepare packet
  memset(lora_TXBUFF, 0, sizeof(lora_TXBUFF));
  Outputstring.getBytes(lora_TXBUFF, Outputstring.length() + 1);
  
  Serial.println("Sending: " + Outputstring);
  rf95.sendAPRS(lora_TXBUFF, Outputstring.length());
  rf95.waitPacketSent();
  Serial.println("Packet sent!");
}
void batt_read() {
  BattVolts = axp.getBattVoltage()/1000; // Use AXP202 for accurate voltage
}

void writedisplaytext(String Header, String L1, String L2, String L3, String L4, String L5, int wait) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextWrap(false);
  
  // Limit text lengths
  if (Header.length() > 15) Header = Header.substring(0, 15);
  if (L1.length() > 15) L1 = L1.substring(0, 15);
  if (L2.length() > 15) L2 = L2.substring(0, 15);
  if (L3.length() > 15) L3 = L3.substring(0, 15);
  if (L4.length() > 15) L4 = L4.substring(0, 15);
  if (L5.length() > 15) L5 = L5.substring(0, 15);
  
  // Display header
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(Header);
  
  // Display content lines
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(L1);
  display.setCursor(0, 32);
  display.print(L2);
  display.setCursor(0, 40);
  display.print(L3);
  display.setCursor(0, 48);
  display.print(L4);
  if (!L5.isEmpty()) {
    display.setCursor(0, 56);
    display.print(L5);
  }
  
  display.display();
  smartDelay(wait);
}

// GPS tracking variables
float last_lat = 0.0;
float last_lon = 0.0;
bool gps_coords_sent = false;
// Operation flags
bool first_start_tx = true;
bool gps_first_valid_tx = false;
unsigned long last_minute_send = 0;
// 按键处理现在由button_utils.cpp中的BUTTON_Utils命名空间负责
void simplifiedInit() {
  // Initialize variables
  for (int i=0; i<ANGLE_AVGS; i++) average_course[i] = 0;
  // Initialize hardware
  pinMode(TXLED, OUTPUT);
  digitalWrite(TXLED, LOW);
  Serial.begin(115200);
  // 初始化按键处理系统
  BUTTON_Utils::setup();
  // Initialize I2C and power management
  Wire.begin(I2C_SDA, I2C_SCL);
  axp.begin(Wire, AXP192_SLAVE_ADDRESS);
  // Enable power outputs
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);   // OLED
  axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);   // GPS
  axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
  axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
  axp.setDCDC1Voltage(3300);
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for(;;);
  }
  // Initialize LoRa module
  if (!rf95.init()) {
    Serial.println("LoRa initialization failed!");
    for(;;);
  }
  // Configure LoRa
  rf95.setFrequency(TXFREQ);
  rf95.setModemConfig(BG_RF95::Bw125Cr45Sf4096);
  rf95.setTxPower(TXdbmW);
  // Initialize GPS
  ss.begin(GPSBaud, SERIAL_8N1, TXPin, RXPin); 
  // Welcome message
  writedisplaytext("LoRa-APRS", "Ready", "Config loaded", "1-click=TX", "", "", 2000);
}
void setup() { simplifiedInit(); }
void loop() {
  // 处理按键事件
  BUTTON_Utils::loop();
  static bool initialized = false;
  if (!initialized) {
    // Display main screen
    batt_read();
    writedisplaytext("LoRa-APRS", CALLSIGN, "Ready", "BAT: " + String(BattVolts, 1) + "V", "", "", 1000);
    initialized = true; 
    // First boot transmission with preset location
    Serial.println("[SYSTEM] First start - Sending preset");
    writedisplaytext("((TX))", "First Boot", "Using Preset Loc", "LAT: " + LATITUDE_PRESET, "LON: " + LONGITUDE_PRESET, "Sending...", 0);
    sendpacketWithPresetLocation();
    writedisplaytext("((TX))", "First Boot", "Sent Successfully", "", "", "", 500);
    lastTX = millis();
    first_start_tx = false;
  }
  // Process GPS data
  while (ss.available() > 0) {
    gps.encode(ss.read());
  }
  LatShown = String(gps.location.lat(),5);
  LongShown = String(gps.location.lng(),5);
  // First valid GPS position transmission
  if (gps.location.isValid() && gps.location.isUpdated()) {
    float current_lat = gps.location.lat();
    float current_lon = gps.location.lng();
    // First valid GPS data
    if (!gps_first_valid_tx && gps.time.isValid()) {
      Serial.println("[SYSTEM] First valid GPS data - Sending");
      writedisplaytext("((TX))","First Valid GPS","LAT: " + LatShown,"LON: " + LongShown,"Sending...","",0);
      sendpacket();
      writedisplaytext("((TX))","First GPS TX","Sent Successfully","","","",500);
      lastTX = millis();
      gps_first_valid_tx = true;
    }
    last_lat = current_lat;
    last_lon = current_lon;
    gps_coords_sent = true;
  }
  // Update smart beaconing values (kept minimal)
  average_speed[point_avg_speed] = gps.speed.kmph();
  point_avg_speed = (point_avg_speed >= 4) ? 0 : point_avg_speed + 1;
  // Periodic transmission every 5 minutes
  unsigned long current_time = millis();
  int current_minute = (current_time / 60000) % 60;
  if (current_minute % 30 == 0 && (current_time - last_minute_send) >= 60000) {
    Serial.println("[SYSTEM] Time-based transmission (every 5 min)");
    digitalWrite(TXLED, HIGH);  
    if (gps.location.age() < 2000) {
      // Use GPS data
      writedisplaytext("((TX))","Periodic TX","LAT: "+LatShown,"LON: "+LongShown,"SPD: "+String(gps.speed.kmph(),1),"Min: "+String(current_minute),0);
      Serial.print("((TX))/Min:" + String(current_minute) + "/LAT:" + LatShown + "/LON:" + LongShown);
    } else {
      // Use preset location
      writedisplaytext("((TX))","Periodic TX","Using Preset Loc","LAT: "+LATITUDE_PRESET,"LON: "+LONGITUDE_PRESET,"Min: "+String(current_minute),0);
      Serial.print("((TX))/Min:" + String(current_minute) + "/Preset");
    }
    sendpacket(); 
    Serial.print(" / BAT: " + String(BattVolts,1));
    digitalWrite(TXLED, LOW);
    last_minute_send = current_time;
    lastTX = current_time;
  } 
  smartDelay(900);
}