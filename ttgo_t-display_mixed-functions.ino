#include <TFT_eSPI.h>
#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include "esp_adc_cal.h"
#include "bmp.h"
#include "settings.h"
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include "GfxUi.h"          

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL          4  // Display backlight control pin
#define ADC_EN          14
#define ADC_PIN         34
#define BUTTON_1        35
#define BUTTON_2        0
#define TFT_HIGHT       135
#define TFT_WIDTH       240

TFT_eSPI tft = TFT_eSPI(TFT_HIGHT, TFT_WIDTH); // Invoke custom library
GfxUi ui = GfxUi(&tft); // Jpeg and bmpDraw function

char buff[512];
int vref = 1100;

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  //entered config mode
}

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms){   
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_ON);
    Serial.printf("%u Going to sleep...\n\r", esp_timer_get_time()/1000 ); 
    Serial.flush();
    esp_light_sleep_start();
    Serial.printf("%u waking up...\n\r", esp_timer_get_time()/1000 ); 
}

void showVoltage(){
    static uint64_t timeStamp = 0;
    if (millis() - timeStamp > 1000) {
        timeStamp = millis();
        uint16_t v = analogRead(ADC_PIN);
        float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
        String voltage = "Voltage :" + String(battery_voltage) + "V";
        Serial.println(voltage);
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(voltage,  tft.width() / 2, tft.height() / 2 );
    }
}

/***************************************************************************************
**                          Update progress bar
***************************************************************************************/
void drawProgress(uint8_t percentage, String text) {
  if ( percentage == 0 ) {
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(TFT_BL, HIGH);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    //tft.setTextPadding(240);
  }

  tft.drawString(text, 70, (TFT_WIDTH/2)-10);
  ui.drawProgressBar(20, (TFT_HIGHT/2) + 10, (TFT_WIDTH - 40), 15, percentage, TFT_WHITE, TFT_BLUE);
  //tft.setTextPadding(0);
}


void setup(){
    Serial.begin(115200);
    Serial.println("Start");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);



    tft.setSwapBytes(true);
    tft.pushImage(0, 0,  240, 135, ttgo);
    
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
    //reset settings - for testing
    //wm.resetSettings();
    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wm.setAPCallback(configModeCallback);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if (!wm.autoConnect(WIFI_HOSTNAME, "pa55w0rd")) {
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(1000);
    }

  
    ArduinoOTA.setHostname(WIFI_HOSTNAME);
    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
  
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      drawProgress((progress / (total / 100)), "OTA Update...");
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) { Serial.println("Auth Failed"); drawProgress(0, "ERROR: OTA Auth Failed"); }
      else if (error == OTA_BEGIN_ERROR) { Serial.println("Begin Failed"); drawProgress(0, "ERROR: OTA Begin Failed"); }
      else if (error == OTA_CONNECT_ERROR) { Serial.println("Connect Failed"); drawProgress(0, "ERROR: OTA Connect Failed"); }
      else if (error == OTA_RECEIVE_ERROR) { Serial.println("Receive Failed"); drawProgress(0, "ERROR: OTA Receive Failed"); }
      else if (error == OTA_END_ERROR) { Serial.println("End Failed"); drawProgress(0, "ERROR: OTA Auth Failed"); }
    });
    // Start the server
    ArduinoOTA.begin();
    
    espDelay(1000);

    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC1_CHANNEL_6, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref:%u mV\n\r", adc_chars.vref);
        vref = adc_chars.vref;
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.println("Default Vref: 1100mV");
    }
    //Turn Off BL
    digitalWrite(TFT_BL, LOW);
}



void loop() {
    //delay(300);
    ArduinoOTA.handle();
}
