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
      
      // SPI Pins for NORVI X CPU
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
      cfg.invert           = true; // ST7789 requires inverted colors
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true; // Required for shared SPI bus
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
// EXPANSION MODULE I2C ADDRESSES 
// ==========================================
#define DI16_ADDR 0x27  
// ==========================================

// --- Objects & State Variables ---
PCA9536 io;
int currentPage = 0; // 0 = DI4, 1 = DI16

bool lastPb1State = HIGH;
bool lastPb2State = HIGH;
unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize X-DI4 GPIO Pins (Using internal pullups just in case)
  pinMode(DI4_IN1, INPUT_PULLUP);
  pinMode(DI4_IN2, INPUT_PULLUP);
  pinMode(DI4_IN3, INPUT_PULLUP);
  pinMode(DI4_IN4, INPUT_PULLUP);

  // Initialize Built-in Buttons
  if (!io.begin()) {
    Serial.println("PCA9536 not found!");
  } else {
    io.pinMode(IO_PB1, INPUT);
    io.pinMode(IO_PB2, INPUT);
  }

  // Initialize TFT Display
  tft.init();
  tft.setRotation(0); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2); 
}

void loop() {
  // --- 1. Button Navigation Logic ---
  bool currentPb1 = io.digitalRead(IO_PB1); // Next
  bool currentPb2 = io.digitalRead(IO_PB2); // Prev

  // Next Page (Button 1 Pressed)
  if (currentPb1 == LOW && lastPb1State == HIGH) {
    currentPage++;
    if (currentPage > 1) currentPage = 0; 
    tft.fillScreen(TFT_BLACK); 
    delay(50); 
  }
  lastPb1State = currentPb1;

  // Previous Page (Button 2 Pressed)
  if (currentPb2 == LOW && lastPb2State == HIGH) {
    currentPage--;
    if (currentPage < 0) currentPage = 1; 
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
      displayDI16();
    }
    
    // UI Navigation Hint 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 260);
    tft.println("--------------------");
    tft.println("[B2: <]      [B1: >]");
  }
}

// --- Functions to Read and Display Each Module ---

void displayDI4() {
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("   X-DI4 Inputs     ");
  tft.println("--------------------");
  
  // INVERT THE LOGIC using '!' 
  // (Changes default '1' to '0' / OFF)
  bool in1 = !digitalRead(DI4_IN1);
  bool in2 = !digitalRead(DI4_IN2);
  bool in3 = !digitalRead(DI4_IN3);
  bool in4 = !digitalRead(DI4_IN4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.printf(" IN 1: %s \n", in1 ? "ON " : "OFF");
  tft.printf(" IN 2: %s \n", in2 ? "ON " : "OFF");
  tft.printf(" IN 3: %s \n", in3 ? "ON " : "OFF");
  tft.printf(" IN 4: %s \n", in4 ? "ON " : "OFF");
  
  for(int i=0; i<4; i++) tft.println("                    "); 
}

void displayDI16() {
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("   X-DI16 Inputs    ");
  tft.println("--------------------");

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
    tft.println(" Check connections. ");
    for(int i=0; i<6; i++) tft.println("                    "); 
    return;
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  for (int i = 0; i < 8; i++) {
    // INVERT THE LOGIC using '!'
    bool stateA = !bitRead(di16_states, i);       
    bool stateB = !bitRead(di16_states, i + 8);   
    
    tft.printf(" IN%02d:%-3s  IN%02d:%-3s\n", 
                i + 1, stateA ? "ON" : "OFF", 
                i + 9, stateB ? "ON" : "OFF");
  }
}