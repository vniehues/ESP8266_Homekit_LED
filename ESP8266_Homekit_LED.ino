/*
 * Example05_WS2812_Neopixel.ino
 *
 *  Created on: 2020-10-08
 *      Author: Vincent Niehues
 *	Thanks to all the other helpful people commenting here.
 *
 * This example allows to change brightness and color of a connected FastLED (SK6812) strip/matrix
 *
 * You should:
 * 1. read and use the Example01_TemperatureSensor with detailed comments
 *    to know the basic concept and usage of this library before other examples。
 * 2. erase the full flash or call homekit_storage_reset() in setup()
 *    to remove the previous HomeKit pairing storage and
 *    enable the pairing with the new accessory of this new HomeKit example.
 */


#include <Arduino.h>
#include <arduino_homekit_server.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include "FastLED_RGBW.h"

// FastLED
#define LED_PIN     5
#define NUM_LEDS    120
#define BRIGHTNESS  255
#define LED_TYPE    SK6812
#define COLOR_ORDER RGB
#define FRAMES_PER_SECOND  120

// FastLED with RGBW
CRGBW leds[NUM_LEDS];
CRGB *ledsRGB = (CRGB *) &leds[0];

uint8_t gHue = 0; // rotating "base color" used by many of the patterns


bool received_sat = false;
bool received_hue = false;

bool rainbow_on = false;

bool is_on = false;
float current_brightness_mapped =  100.0;
float current_brightness =  255.0;
float current_sat = 0.0;
float current_hue = 0.0;
int rgb_colors[3];
int rgbw_colors[4];

CRGBPalette16 currentPalette;
TBlendType    currentBlending;

const IPAddress apIP(192, 168, 1, 1);
const char* apSSID = "Nice_Lamp_Setup";
boolean settingMode;
String ssidList;

DNSServer dnsServer;
ESP8266WebServer webServer(80);

void setup() {

  Serial.begin(115200);
  
  EEPROM.begin(512);
  
  delay(500);

  // Read config from EEPROM
  if (restoreConfig()) {
    // Connect to WiFi
    if (checkConnection()) {
      settingMode = false;

      // Init LEDs
      FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(ledsRGB, getRGBWsize(NUM_LEDS));
      FastLED.setBrightness(current_brightness_mapped);
      FastLED.show();
      FastLED.clear();
 
      delay(500);

      rgb_colors[0] = 255;
      rgb_colors[1] = 255;
      rgb_colors[2] = 255;

      rgbw_colors[0] = 0;
      rgbw_colors[1] = 0;
      rgbw_colors[2] = 0;
      rgbw_colors[3] = 255;
  
      // Start homekit setup
      my_homekit_setup();
      return;
    }
  }

  // Enter WifiSetup, if one of the above fails
  settingMode = true;
  StartWifiSetup();
}

//==============================
// HomeKit setup and loop
//==============================

// access your HomeKit characteristics defined in my_accessory.c

extern "C" homekit_server_config_t accessory_config;
extern "C" homekit_characteristic_t cha_on;
extern "C" homekit_characteristic_t cha_bright;
extern "C" homekit_characteristic_t cha_sat;
extern "C" homekit_characteristic_t cha_hue;

extern "C" homekit_characteristic_t cha_switch_on;
extern "C" homekit_characteristic_t cha_rainbow_on;

static uint32_t next_heap_millis = 0;

void my_homekit_setup() {

  // Comment in, when pairing fails
  // homekit_storage_reset();

  cha_on.setter = set_on;
  cha_bright.setter = set_bright;
  cha_sat.setter = set_sat;
  cha_hue.setter = set_hue;

  cha_switch_on.setter = set_rainbow_on;
  cha_rainbow_on.setter = set_rainbow_on;
  
	arduino_homekit_setup(&accessory_config);
}

// the continuous loop. 
// homekit loop needs to be the default with 
// regular calls for other things.
void loop() {
  if (settingMode) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }
  else
  {
	  arduino_homekit_loop();
    EVERY_N_MILLISECONDS(50){Run_Additionals();}
  }
}

void Run_Additionals()
{
  if(rainbow_on)
  {
    static uint8_t startIndex = 0;
    startIndex++;
    FillLedsWithColors(startIndex);
    FastLED.setBrightness(current_brightness_mapped);
    FastLED.show(); 
  }
}

void FillLedsWithColors( uint8_t colorIndex)
{
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(currentPalette, colorIndex, 255, currentBlending);
        colorIndex++;
    }
}

void set_rainbow_on(const homekit_value_t v) {
    bool on = v.bool_value;
    cha_switch_on.value.bool_value = on; //sync the value

    rainbow_on = on;

    if(on) {
        Serial.println("Rainbow on");
        currentPalette = RainbowStripeColors_p;         
        currentBlending = LINEARBLEND;
    } else  {
        Serial.println("Rainbow off");
        updateColor();
    }
}

void set_on(const homekit_value_t v) {
    bool on = v.bool_value;
    cha_on.value.bool_value = on; //sync the value

    if(on) {
        is_on = true;
        Serial.println("On");
    } else  {
        is_on = false;
        set_rainbow_on(v);
        Serial.println("Off");
    }

    if(!rainbow_on)
    {   
      updateColor();
    }
}

void set_hue(const homekit_value_t v) {
    Serial.println("set_hue");
    float hue = v.float_value;
    cha_hue.value.float_value = hue; //sync the value

    current_hue = hue;
    received_hue = true;
    
    if(!rainbow_on)
    {    
      updateColor();
    }
}

void set_sat(const homekit_value_t v) {
    Serial.println("set_sat");
    float sat = v.float_value;
    cha_sat.value.float_value = sat; //sync the value

    current_sat = sat;
    received_sat = true;
    
    if(!rainbow_on)
    {    
      updateColor();
    }
}

void set_bright(const homekit_value_t v) {
    int bright = v.int_value;
    cha_bright.value.int_value = bright; //sync the value

    current_brightness = bright;
    
    int b = map(bright,0, 100,5, 255);  
    current_brightness_mapped = b;
      
    if(!rainbow_on)
    {    
      updateColor();
    }
}

void updateColor()
{
  if(is_on)
  {
      if(received_hue && received_sat)
      {
        HSV2RGB(current_hue, current_sat, current_brightness);
        rgb2rgbw(rgb_colors[0],rgb_colors[1],rgb_colors[2]);
        received_hue = false;
        received_sat = false;
      }
      
    if(!rainbow_on)
    {   
      FastLED.setBrightness(current_brightness_mapped);
    }
      
      for(int i = 0; i < NUM_LEDS; i++)
      {
        leds[i] = CRGBW(rgbw_colors[0],rgbw_colors[1],rgbw_colors[2],rgbw_colors[3] );
      }
      
      FastLED.show();
    }
  else if(!is_on) //lamp - switch to off
  {
      Serial.println("is_on == false");
      FastLED.setBrightness(0);      
      FastLED.clear();
      FastLED.show();
  }
}

void HSV2RGB(float h,float s,float v) {

  int i;
  float m, n, f;

  s/=100;
  v/=100;

  if(s==0){
    rgb_colors[0]=rgb_colors[1]=rgb_colors[2]=round(v*255);
    return;
  }

  h/=60;
  i=floor(h);
  f=h-i;

  if(!(i&1)){
    f=1-f;
  }

  m=v*(1-s);
  n=v*(1-s*f);

  switch (i) {

    case 0: case 6:
      rgb_colors[0]=round(v*255);
      rgb_colors[1]=round(n*255);
      rgb_colors[2]=round(m*255);
    break;

    case 1:
      rgb_colors[0]=round(n*255);
      rgb_colors[1]=round(v*255);
      rgb_colors[2]=round(m*255);
    break;

    case 2:
      rgb_colors[0]=round(m*255);
      rgb_colors[1]=round(v*255);
      rgb_colors[2]=round(n*255);
    break;

    case 3:
      rgb_colors[0]=round(m*255);
      rgb_colors[1]=round(n*255);
      rgb_colors[2]=round(v*255);
    break;

    case 4:
      rgb_colors[0]=round(n*255);
      rgb_colors[1]=round(m*255);
      rgb_colors[2]=round(v*255);
    break;

    case 5:
      rgb_colors[0]=round(v*255);
      rgb_colors[1]=round(m*255);
      rgb_colors[2]=round(n*255);
    break;
  }
}

#include "math.h"

void rgb2rgbw(int R, int G, int B)
{

  int minRG = min(R,G);
  int minRGB = min(minRG, B);

  int newR = R - minRGB;
  int newG = G - minRGB;
  int newB = B - minRGB;
  int newW = minRGB;

      rgbw_colors[0]= newR;
      rgbw_colors[1]=newG;
      rgbw_colors[2]=newB;
      rgbw_colors[3]=newW;
}

















boolean restoreConfig() {
  Serial.println("Reading EEPROM...");
  String ssid = "";
  String pass = "";
  if (EEPROM.read(364) != 0) {
    for (int i = 364; i < 396; ++i) {
      ssid += char(EEPROM.read(i));
    }
    Serial.print("SSID: ");
    Serial.println(ssid);
    for (int i = 396; i < 460; ++i) {
      pass += char(EEPROM.read(i));
    }
    Serial.print("Password: ");
    Serial.println(pass);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.print("SSID: ");
    Serial.println(ssid.c_str());
    Serial.print("Password: ");
    Serial.println(pass.c_str());

    return true;
  }
  else {
    Serial.println("Config not found.");
    return false;
  }
}

boolean checkConnection() {
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  while ( count < 30 ) {
    if (WiFi.isConnected()) {
      Serial.println();
      Serial.println("Connected!");
      return (true);
    }
    delay(1000);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  return false;
}

void startWebServer() {
  if (settingMode) {
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.softAPIP());
    webServer.on("/settings", []() {
      String s = "<h1>Wi-Fi Settings</h1><p>Please enter your password by selecting the SSID.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += ssidList;
      s += "</select><br>Password: <input name=\"pass\" length=64 type=\"password\"><input type=\"submit\"></form>";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    webServer.on("/setap", []() {
      for (int i = 364; i < 460; ++i) {
        EEPROM.put(i, 0);
      }
      String ssid = urlDecode(webServer.arg("ssid"));
      Serial.print("SSID: ");
      Serial.println(ssid);
      String pass = urlDecode(webServer.arg("pass"));
      Serial.print("Password: ");
      Serial.println(pass);
      Serial.println("Writing SSID to EEPROM...");
      for (int i = 0; i < ssid.length(); ++i) {
        EEPROM.put(i+364, ssid[i]);
      }
      Serial.println("Writing Password to EEPROM...");
      for (int i = 0; i < pass.length(); ++i) {
        EEPROM.put(396 + i, pass[i]);
      }
      EEPROM.commit();
      Serial.println("Write EEPROM done!");
      String s = "<h1>Setup complete.</h1><p>device will be connected to \"";
      s += ssid;
      s += "\" after the restart.";
      webServer.send(200, "text/html", makePage("Wi-Fi Settings", s));
      ESP.restart();
    });
    webServer.onNotFound([]() {
      String s = "<h1>AP mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("AP mode", s));
    });
  }
  else {
    Serial.print("Starting Web Server at ");
    Serial.println(WiFi.localIP());
    webServer.on("/", []() {
      String s = "<h1>STA mode</h1><p><a href=\"/reset\">Reset Wi-Fi Settings</a></p>";
      webServer.send(200, "text/html", makePage("STA mode", s));
    });
    webServer.on("/reset", []() {
      for (int i = 364; i < 460; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      String s = "<h1>Wi-Fi settings was reset.</h1><p>Please reset device.</p>";
      webServer.send(200, "text/html", makePage("Reset Wi-Fi Settings", s));
    });
  }
  webServer.begin();
}

void StartWifiSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  Serial.println("");
  for (int i = 0; i < n; ++i) {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    ssidList += "\">";
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
  }
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID);
  dnsServer.start(53, "*", apIP);
  startWebServer();
  Serial.print("Starting Access Point at \"");
  Serial.print(apSSID);
  Serial.println("\"");
}

// Helper which imbedds the given title & content into the skeleton of an HTML page
String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

// Helper to decode an url-string to a usable string
String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}
