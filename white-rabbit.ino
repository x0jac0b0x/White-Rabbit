/*
  Project: WhiteRabbit 2.4ghz (wifi/bluetooth) Jammer
  Creator: x0jacob0x

  Special thanks to the RF-Clown project
  by cifertech:
  https://github.com/cifertech/RF-Clown

  Inspired by RF-Clown for educational RF research.
*/

#include "RF24.h"
#include <SPI.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include <Adafruit_NeoPixel.h>

constexpr int SPI_SPEED = 8000000;

// Shared SPI bus
constexpr int SCK_PIN  = 6;
constexpr int MOSI_PIN = 7;
constexpr int MISO_PIN = 2;

// NRF-1 (LEFT)
constexpr int CE1_PIN  = 10;
constexpr int CSN1_PIN = 17;

// NRF-2 (RIGHT)
constexpr int CE2_PIN  = 11;
constexpr int CSN2_PIN = 16;

#define BOOT_BUTTON 9

bool wifiMode = false;  // false = Bluetooth/BLE, true = WiFi
bool lastButtonState = HIGH;
unsigned long lastPressTime = 0;
const unsigned long debounceDelay = 250;

SPIClass *spiFSPI = nullptr;

RF24 radio1(CE1_PIN, CSN1_PIN, SPI_SPEED);
RF24 radio2(CE2_PIN, CSN2_PIN, SPI_SPEED);

// 2.4 GHz Wi-Fi channel centers, US channels 1–11 (1,6,11 don't overlap)
const byte WiFi_channels[] = {
    12,  // Wi-Fi 1  = 2412 MHz
    17,  // Wi-Fi 2  = 2417 MHz
    22,  // Wi-Fi 3  = 2422 MHz
    27,  // Wi-Fi 4  = 2427 MHz
    32,  // Wi-Fi 5  = 2432 MHz
    37,  // Wi-Fi 6  = 2437 MHz
    42,  // Wi-Fi 7  = 2442 MHz
    47,  // Wi-Fi 8  = 2447 MHz
    52,  // Wi-Fi 9  = 2452 MHz
    57,  // Wi-Fi 10 = 2457 MHz
    62   // Wi-Fi 11 = 2462 MHz
};

const byte bluetooth_channels[] = {
    32,  // 2432 MHz — BLE data channel 13
    34,  // 2434 MHz — BLE data channel 14
    46,  // 2446 MHz — BLE data channel 20
    48,  // 2448 MHz — BLE data channel 21
    50,  // 2450 MHz — BLE data channel 22
    52,  // 2452 MHz — BLE data channel 23
     2,  // 2402 MHz — BLE advertising channel 37
     4,  // 2404 MHz — BLE data channel 0
     6,  // 2406 MHz — BLE data channel 1
     8,  // 2408 MHz — BLE data channel 2
    22,  // 2422 MHz — BLE data channel 9
    24,  // 2424 MHz — BLE data channel 10
    26,  // 2426 MHz — BLE advertising channel 38
    28,  // 2428 MHz — BLE data channel 11
    30,  // 2430 MHz — BLE data channel 12
    74,  // 2474 MHz — BLE data channel 34
    76,  // 2476 MHz — BLE data channel 35
    78,  // 2478 MHz — BLE data channel 36
    80   // 2480 MHz — BLE advertising channel 39
};

#define RGB_LED_PIN 8
#define NUM_LEDS 1

Adafruit_NeoPixel rgb(NUM_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

bool configureRadio(RF24 &radio, int channel, SPIClass *spi);

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(BOOT_BUTTON, INPUT_PULLUP);

    rgb.begin();
    rgb.setBrightness(64);

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_bt_controller_deinit();

    spiFSPI = new SPIClass(FSPI);
    spiFSPI->begin(SCK_PIN, MISO_PIN, MOSI_PIN);

    bool radio1OK = configureRadio(
        radio1,
        bluetooth_channels[0],
        spiFSPI
    );

    bool radio2OK = configureRadio(
        radio2,
        bluetooth_channels[0],
        spiFSPI
    );

    if (radio1OK && radio2OK) {
        // Both radios passed: green
        rgb.setPixelColor(0, rgb.Color(0, 255, 0));
        rgb.show();
    } else {
        // One or both radios failed: flashing red
        while (true) {
            rgb.setPixelColor(0, rgb.Color(255, 0, 0));
            rgb.show();
            delay(500);

            rgb.setPixelColor(0, rgb.Color(0, 0, 0));
            rgb.show();
            delay(500);
        }
    }
    
    while (digitalRead(BOOT_BUTTON) == HIGH) {
        delay(10);
    }

    while (digitalRead(BOOT_BUTTON) == LOW) {
        delay(10);
    }
    lastButtonState = HIGH;
    lastPressTime = millis() - debounceDelay;
    rgb.setPixelColor(0, rgb.Color(0, 0, 255));
    rgb.show();
}

bool configureRadio(RF24 &radio, int channel, SPIClass *spi) {
    if (!radio.begin(spi)) {
        return false;
    }

    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.startConstCarrier(RF24_PA_MAX, channel);
    return true;
}

void loop() {
    bool buttonState = digitalRead(BOOT_BUTTON);

    if (buttonState == LOW && lastButtonState == HIGH) {
        if (millis() - lastPressTime > debounceDelay) {
            wifiMode = !wifiMode;

            if (wifiMode) {
                rgb.setPixelColor(0, rgb.Color(255, 0, 0));  // WiFi = red
            } else {
                rgb.setPixelColor(0, rgb.Color(0, 0, 255));  // Bluetooth/BLE = blue
            }

            rgb.show();
            lastPressTime = millis();
        }
    }

    lastButtonState = buttonState;

    if (wifiMode) {
        jamWiFi();
    } else {
        jamBluetooth();
    }
}

void jamWiFi() {
    static int index = 0;

    constexpr size_t count = 
        sizeof (WiFi_channels) / sizeof(WiFi_channels[0]);

    const size_t index2 = (index + count / 2) % count;

    radio1.setChannel(WiFi_channels[index]);
    radio2.setChannel(WiFi_channels[index2]);

    index = (index + 1) % count;
}
void jamBluetooth() {
    static size_t index = 0;

    constexpr size_t count =
        sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]);

    const size_t index2 = (index + count / 2) % count;

    radio1.setChannel(bluetooth_channels[index]);
    radio2.setChannel(bluetooth_channels[index2]);

    index = (index + 1) % count;
}
