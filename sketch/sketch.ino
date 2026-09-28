#include <Wire.h>
#include <SPI.h>
#include <PCA9536D.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// --- LovyanGFX Display Configuration for NORVI X ---
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 12; 
      cfg.pin_mosi = 11; 
      cfg.pin_miso = 13; 
      cfg.pin_dc   = 46; 
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 45; 
      cfg.pin_rst          = 47; 
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = true; 
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true; 
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft; 

// --- Pin Definitions ---
#define SDA_PIN 8
#define SCL_PIN 9

// X-DI4 Direct GPIO Pins
#define DI4_IN1 5
#define DI4_IN2 6
#define DI4_IN3 7
#define DI4_IN4 10

// PCA9536 Built-in Buttons (I2C 0x41)
#define IO_PB1  0  // "Next" Button
#define IO_PB2  3  // "Previous" Button

// ==========================================
// ⚠️ EXPANSION MODULE I2C ADDRESSES ⚠️
// If DI8 or DI16 is "Not Found", check the 
// Serial Monitor on boot to find the correct 
// address and update these numbers:
// ==========================================
#define DI8_ADDR  0x73  
#define DI16_ADDR 0x74  // Common alternatives: 0x76, 0x77, or 0x20
// ==========================================

// --- Objects & State Variables ---
PCA9536 io;
int currentPage = 0; // 0 = DI4, 1 = DI8, 2 = DI16

bool lastPb1State = HIGH;
bool lastPb2State = HIGH;
unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // --- I2C Auto-Scanner ---
  Serial.println("\n--- I2C Scanner ---");
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found I2C device at address: 0x");
      Serial.println(i, HEX);
    }
  }
  Serial.println("-------------------\n");

  // Initialize X-DI4 GPIO Pins
  pinMode(DI4_IN1, INPUT);
  pinMode(DI4_IN2, INPUT);
  pinMode(DI4_IN3, INPUT);
  pinMode(DI4_IN4, INPUT);

  // Initialize Built-in Buttons
  if (!io.begin()) {
    Serial.println("PCA9536 not found!");
  } else {
    io.pinMode(IO_PB1, INPUT);
    io.pinMode(IO_PB2, INPUT);
  }

  // Initialize TFT Display
  tft.init();
  tft.setRotation(0); // 0 = Portrait Mode (240x320)
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2); // Fits exactly 20 characters per line
}

void loop() {
  // --- 1. Button Navigation Logic ---
  bool currentPb1 = io.digitalRead(IO_PB1); // Next
  bool currentPb2 = io.digitalRead(IO_PB2); // Prev

  // Next Page (Button 1 Pressed)
  if (currentPb1 == LOW && lastPb1State == HIGH) {
    currentPage++;
    if (currentPage > 2) currentPage = 0; 
    tft.fillScreen(TFT_BLACK); 
    delay(50); 
  }
  lastPb1State = currentPb1;

  // Previous Page (Button 2 Pressed)
  if (currentPb2 == LOW && lastPb2State == HIGH) {
    currentPage--;
    if (currentPage < 0) currentPage = 2; 
    tft.fillScreen(TFT_BLACK); 
    delay(50); 
  }
  lastPb2State = currentPb2;


  // --- 2. Update Display (Every 100ms) ---
  if (millis() - lastDisplayUpdate >= 100) {
    lastDisplayUpdate = millis();
    tft.setCursor(0, 5);

    if (currentPage == 0) {
      displayDI4();
    } else if (currentPage == 1) {
      displayDI8();
    } else if (currentPage == 2) {
      displayDI16();
    }
    
    // UI Navigation Hint for Portrait (Max 20 chars)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 260);
    tft.println("--------------------");
    tft.println("[B2: <]    [B1: >]");
  }
}

// --- Functions to Read and Display Each Module ---

void displayDI4() {
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("   X-DI4 Inputs     ");
  tft.println("--------------------");
  
  // Read direct GPIO pins
  bool in1 = digitalRead(DI4_IN1);
  bool in2 = digitalRead(DI4_IN2);
  bool in3 = digitalRead(DI4_IN3);
  bool in4 = digitalRead(DI4_IN4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf(" IN 1: %s \n", in1 ? "ON " : "OFF");
  tft.printf(" IN 2: %s \n", in2 ? "ON " : "OFF");
  tft.printf(" IN 3: %s \n", in3 ? "ON " : "OFF");
  tft.printf(" IN 4: %s \n", in4 ? "ON " : "OFF");
  
  // Padding to clear screen leftover space
  for(int i=0; i<4; i++) tft.println("                    "); 
}

void displayDI8() {
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("   X-DI8 Inputs     ");
  tft.println("--------------------");

  // Read 1 Byte from I2C
  uint8_t di8_states = 0;
  Wire.beginTransmission(DI8_ADDR);
  Wire.write(0x00); 
  if (Wire.endTransmission() == 0) {
    Wire.requestFrom(DI8_ADDR, 1);
    if (Wire.available()) {
      di8_states = Wire.read();
    }
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println(" X-DI8 Not Found!   ");
    for(int i=0; i<7; i++) tft.println("                    "); 
    return;
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < 8; i++) {
    bool state = bitRead(di8_states, i);
    tft.printf(" IN %d: %s \n", i + 1, state ? "ON " : "OFF");
  }
}

void displayDI16() {
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("   X-DI16 Inputs    ");
  tft.println("--------------------");

  // Read 2 Bytes from I2C 
  uint16_t di16_states = 0;
  Wire.beginTransmission(DI16_ADDR);
  Wire.write(0x00); 
  if (Wire.endTransmission() == 0) {
    Wire.requestFrom(DI16_ADDR, 2);
    if (Wire.available() == 2) {
      uint8_t port0 = Wire.read();
      uint8_t port1 = Wire.read();
      di16_states = (port1 << 8) | port0; 
    }
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println(" X-DI16 Not Found!  ");
    tft.println(" Check Serial Mon.  ");
    for(int i=0; i<6; i++) tft.println("                    "); 
    return;
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Display tightly in two columns for Portrait Mode
  // Example output: " IN01:ON   IN09:OFF"
  for (int i = 0; i < 8; i++) {
    bool stateA = bitRead(di16_states, i);       
    bool stateB = bitRead(di16_states, i + 8);   
    
    tft.printf(" IN%02d:%-3s  IN%02d:%-3s\n", 
                i + 1, stateA ? "ON" : "OFF", 
                i + 9, stateB ? "ON" : "OFF");
  }
}