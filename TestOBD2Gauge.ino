#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <CST816S.h>
#include "ui.h"
#include <WiFi.h>
#include "ELMduino.h"

// ELMDUINO
const char *ssid = "V-LINK";
IPAddress server(192, 168, 0, 10);
WiFiClient client;
ELM327 myELM327;

// TIMER E STATI
uint32_t last_obd_request_time = 0;
const uint32_t obd_interval = 5000;  // 5 secondi come richiesto
enum TestState { SEND_WATER,
                 READ_WATER,
                 REINIT_BUS };
TestState currentTest = SEND_WATER;
uint8_t consecutive_errors = 0;

// PIN SCHERMO (Invariati)
#define TFT_SCLK 6
#define TFT_MOSI 7
#define TFT_CS 10
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 3
#define TOUCH_SDA 4
#define TOUCH_SCL 5
#define TOUCH_INT 0
#define TOUCH_RST 1

CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, 40000000);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, 0, true);

static const uint32_t screenWidth = 240;
static const uint32_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 30];

// --- FUNZIONI DI SUPPORTO ---

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (touch.available()) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch.data.x;
    data->point.y = touch.data.y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void WaterTemperatureReading(int temp) {
  if (ui_WaterTemperatureArc != NULL) {
    lv_arc_set_value(ui_WaterTemperatureArc, temp);
    lv_event_send(ui_WaterTemperatureArc, LV_EVENT_VALUE_CHANGED, NULL);
    // Logica colori semplificata
    if (temp >= 115) {
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    } else if (temp <= 70) {
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    } else {
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, lv_color_make(0, 180, 50), LV_PART_INDICATOR);
    }
  }
}

// --- LOGICA OBD CORAZZATA ---

void reinit_sequence() {
  Serial.println("\n[SYSTEM] Esecuzione Reinit Hard...");
  while (client.available()) client.read();
  myELM327.sendCommand("ATZ");
  delay(1500);  // Reset ELM
  myELM327.sendCommand("ATE0");
  delay(300);  // Echo Off
  myELM327.sendCommand("ATS0");
  delay(300);  // Spazi Off
  myELM327.sendCommand("ATSP5");
  delay(1000);  // BMW Protocol
  myELM327.sendCommand("0100");
  delay(1000);  // Wake up bus
  consecutive_errors = 0;
}

void test_obd_readings() {
  static uint32_t last_attempt_time = 0;

  switch (currentTest) {
    case SEND_WATER:
      if (millis() - last_obd_request_time < obd_interval) return;

      while (client.available()) client.read();  // Svuota buffer prima dell'invio
      Serial.println("\n[OBD] Invio 0105...");
      myELM327.sendCommand("0105");
      last_attempt_time = millis();
      currentTest = READ_WATER;
      break;

    case READ_WATER:
      if (myELM327.get_response() == ELM_SUCCESS) {
        char responseStr[64] = { 0 };
        for (int i = 0; i < 60; i++) responseStr[i] = (char)myELM327.payload[i];

        Serial.printf("[OBD] Raw: %s\n", responseStr);

        // Cerchiamo il codice risposta "4105" ignorando l'echo
        char *pos = strstr(responseStr, "4105");
        if (pos != NULL && strlen(pos) >= 6) {
          char hexVal[3] = { pos[4], pos[5], '\0' };
          int raw_val = (int)strtol(hexVal, NULL, 16);
          int temp = raw_val - 40;

          if (temp > -30 && temp < 150) {
            Serial.printf(">>> ACQUA OK: %d C\n", temp);
            WaterTemperatureReading(temp);
            consecutive_errors = 0;
          }
        }
        last_obd_request_time = millis();
        currentTest = SEND_WATER;
      } else if (millis() - last_attempt_time > 4000) {
        Serial.println("[OBD] Errore/Timeout!");
        consecutive_errors++;
        last_obd_request_time = millis();

        if (consecutive_errors >= 2) {
          currentTest = REINIT_BUS;
        } else {
          currentTest = SEND_WATER;
        }
      }
      break;

    case REINIT_BUS:
      reinit_sequence();
      currentTest = SEND_WATER;
      last_obd_request_time = millis();
      break;
  }
}

// --- SETUP E LOOP ---

void gestione_errori() {
  lv_scr_load_anim(ui_ErrorScreen, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  unsigned long start_time = millis();
  while (millis() - start_time < 500) {  // Ciclo di mezzo secondo
    lv_timer_handler();
    delay(5);
  }
  while (1)
    ;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);

  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 255);

  lv_init();

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 30);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(20);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);
  touch.begin();

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();

  Serial.println("Setup display completato!");

  WiFi.begin(ssid);

  int MaxmsRetry = 20000;
  int startMillis = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMillis <= MaxmsRetry) {
    lv_timer_handler();
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nErrore Connessione Wifi!");
    gestione_errori();
  }

  Serial.println("Connected to WiFi!");

  int attemptNumber = 0;
  bool clientConnected = false;
  while (attemptNumber < 5 && clientConnected == false) {
    if (client.connect(server, 35000)) {
      clientConnected = true;
      Serial.println("Client connected!");
    } else {
      lv_timer_handler();
      Serial.println("Error client retrying...");
      attemptNumber = attemptNumber + 1;
      delay(1000);
    }
  }

  if (!clientConnected) {
    Serial.println("Errore Connessione Client!");
    gestione_errori();
  }

  int ElmAttemptNumber = 0;
  bool ElmConnected = false;
  while (ElmAttemptNumber < 5 && ElmConnected == false) {
    if (!myELM327.begin(client, true, 4000)) {
      lv_timer_handler();
      Serial.println("error elm retrying...");
      ElmAttemptNumber = ElmAttemptNumber + 1;
      delay(1000);
    } else {
      Serial.println("ELM connected!");
      ElmConnected = true;
    }
  }

  if (!ElmConnected) {
    Serial.println("Errore Connessione ELM!");
    gestione_errori();
  }
  reinit_sequence();
}

void loop() {
  lv_timer_handler();
  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    test_obd_readings();
  }
  delay(5);
}