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

// PARAMETRI CONNESSIONE
int connection_attempts = 0;
const int max_attempts = 15;  // Proverà per circa 20-30 secondi totali
bool obd_ready = false;

// PIN SCHERMO
#define TFT_SCLK 6
#define TFT_MOSI 7
#define TFT_CS 10
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 3

// PIN TOUCH
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

bool connect_to_obd() {
  Serial.printf("Tentativo %d di %d...\n", connection_attempts + 1, max_attempts);

  // 1. Verifica/Connetti WiFi
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid);
    unsigned long startWait = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startWait < 5000) {
      lv_timer_handler();  // Mantieni la UI attiva durante l'attesa
      delay(10);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK, provo socket client...");
    if (client.connect(server, 35000)) {
      Serial.println("Socket OK, inizializzo ELM327...");
      if (myELM327.begin(client, true, 2000)) {
        // Inviamo un comando AT per testare se il chip risponde davvero
        myELM327.sendCommand("ATI");
        delay(500);
        if (myELM327.get_response() == ELM_SUCCESS) {
          Serial.println("Sistema OBD Pronto!");
          return true;
        }
      }
    }
  }

  connection_attempts++;
  return false;
}

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

//region sensor readings

void OilTemperatureReading(int temp) {
  if (ui_OilTemperatureArc != NULL) {
    lv_arc_set_value(ui_OilTemperatureArc, temp);
    lv_event_send(ui_OilTemperatureArc, LV_EVENT_VALUE_CHANGED, NULL);

    if (temp >= 130) {
      bool blink = (millis() % 500) < 250;
      lv_obj_set_style_arc_color(ui_OilTemperatureArc, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(34, 34, 34), LV_PART_MAIN);
      lv_obj_set_style_arc_color(ui_OilTemperatureArc, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(34, 34, 34), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_OilTemperatureValue, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(255, 255, 255), 0);
    } else {
      lv_obj_set_style_arc_color(ui_OilTemperatureArc, lv_color_make(34, 34, 34), LV_PART_MAIN);
    }

    if (temp <= 60) {
      lv_obj_set_style_arc_color(ui_OilTemperatureArc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_OilTemperatureValue, lv_color_make(255, 255, 255), 0);
    } else if (temp > 60 && temp <= 80) {
      lv_obj_set_style_arc_color(ui_OilTemperatureArc, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_OilTemperatureValue, lv_color_make(255, 255, 255), 0);
    } else if (temp > 80 && temp <= 110) {
      lv_obj_set_style_arc_color(ui_OilTemperatureArc, lv_color_make(0, 180, 50), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_OilTemperatureValue, lv_color_make(255, 255, 255), 0);
    } else if (temp > 110 && temp < 130) {
      lv_obj_set_style_arc_color(ui_OilTemperatureArc, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_OilTemperatureValue, lv_palette_main(LV_PALETTE_ORANGE), 0);
    }
  }
}

void WaterTemperatureReading(int temp) {
  if (ui_WaterTemperatureArc != NULL) {
    lv_arc_set_value(ui_WaterTemperatureArc, temp);
    lv_event_send(ui_WaterTemperatureArc, LV_EVENT_VALUE_CHANGED, NULL);

    if (temp >= 115) {
      bool blink = (millis() % 500) < 250;
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(34, 34, 34), LV_PART_MAIN);
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(34, 34, 34), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_WaterTemperatureValue, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(255, 255, 255), 0);
    } else {
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, lv_color_make(34, 34, 34), LV_PART_MAIN);
    }

    if (temp <= 70) {
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_WaterTemperatureValue, lv_color_make(255, 255, 255), 0);
    } else if (temp > 70 && temp < 115) {
      lv_obj_set_style_arc_color(ui_WaterTemperatureArc, lv_color_make(0, 180, 50), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_WaterTemperatureValue, lv_color_make(255, 255, 255), 0);
    }
  }
}


//TESTING ----------------------------------------------------
// Timer per l'intervallo di 5 secondi
uint32_t last_obd_request_time = 0;
const uint32_t obd_interval = 5000;

// Stati per alternare i due valori
enum TestState { READ_WATER,
                 READ_OIL,
                 IDLE };
TestState currentTest = READ_WATER;
void test_obd_readings() {
  // Se non sono passati 5 secondi, non fare nulla (IDLE)
  if (millis() - last_obd_request_time < obd_interval && currentTest == READ_WATER) {
    return;
  }

  switch (currentTest) {
    case READ_WATER:
      {
        float waterTemp = myELM327.engineCoolantTemp();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {

          Serial.print("Temp Acqua: ");
          Serial.println(waterTemp);

          WaterTemperatureReading((int)waterTemp);

          currentTest = READ_OIL;  // Passa all'olio
        } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
          currentTest = READ_OIL;  // Salta se errore
        }
        break;
      }
    case READ_OIL:
      {
        // Comando specifico per Temperatura Olio BMW M54
        float oilTemp = myELM327.oilTemp();
        if (myELM327.nb_rx_state == ELM_SUCCESS) {

          Serial.print("Temp Olio: ");
          Serial.println(oilTemp);

          OilTemperatureReading((int)oilTemp);

          last_obd_request_time = millis();  // Reset timer dei 5 secondi
          currentTest = READ_WATER;          // Ricomincia il giro
        } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
          last_obd_request_time = millis();
          currentTest = READ_WATER;
        }
        break;
      }
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

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

  Serial.println("Setup completato!");
  while (!obd_ready && connection_attempts < max_attempts) {
    if (connect_to_obd()) {
      obd_ready = true;
    } else {
      delay(1000);  // Aspetta un secondo prima del prossimo tentativo
    }
    lv_timer_handler();  // Permette alle animazioni di caricamento di girare
  }

  // 3. Gestione finale
  if (obd_ready) {
    Serial.println("Tutto pronto, vado alla dashboard.");
    lv_scr_load_anim(ui_OilWaterTemperatureGauge, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  } else {
    Serial.println("Connessione fallita definitivamente.");
    // Carica una schermata di errore o scrivi un messaggio
    lv_scr_load_anim(ui_ErrorScreen, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  }
}

void loop() {
  lv_timer_handler();

  if (WiFi.status() == WL_CONNECTED) {
    test_obd_readings();
  }

  delay(5);
}