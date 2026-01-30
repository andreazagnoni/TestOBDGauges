#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <CST816S.h>
#include "ui.h"
#include <WiFi.h>
#include "ELMduino.h"

const bool debug = true;
// ELMDUINO
const char *ssid = "V-LINK";
IPAddress server(192, 168, 0, 10);
WiFiClient client;
ELM327 myELM327;

// TIMER E STATI
uint32_t last_obd_request_time = 0;
const uint32_t obd_interval_water = 5000;
const uint32_t obd_interval_engine = 1000;

enum TestState { SEND_WATER,
                 READ_WATER,
                 SEND_RPM,
                 READ_RPM,
                 SEND_KMH,
                 READ_KMH,
                 SEND_ENGINE_LOAD,
                 READ_ENGINE_LOAD,
                 SEND_TORQUE,
                 READ_TORQUE,
                 SEND_HP,
                 READ_HP,
                 SEND_THROTTLE,
                 READ_THROTTLE,
                 SEND_MAF,
                 READ_MAF,
                 SEND_STFT1,
                 READ_STFT1,
                 SEND_STFT2,
                 READ_STFT2,
                 SEND_ANTICIPO,
                 READ_ANTICIPO,
                 SEND_TEMP_ARIA,
                 READ_TEMP_ARIA,
                 SEND_VOLTS,
                 READ_VOLTS,
                 SEND_TEMP_CAT1,
                 READ_TEMP_CAT1,
                 SEND_TEMP_CAT2,
                 READ_TEMP_CAT2,
                 SEND_PRESS_BAR,
                 READ_PRESS_BAR,
                 REINIT_ELM };
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
void RPMReading(int rpm) {
  if (ui_RPMBar != NULL) {
    lv_slider_set_value(ui_RPMBar, rpm, LV_ANIM_ON);
    lv_event_send(ui_RPMBar, LV_EVENT_VALUE_CHANGED, NULL);

    if (rpm >= 6250) {
      bool blink = (millis() % 500) < 250;
      lv_obj_set_style_bg_color(ui_RPMBar, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(34, 34, 34), LV_PART_MAIN);
      lv_obj_set_style_bg_color(ui_RPMBar, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(34, 34, 34), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_RPMValue, blink ? lv_palette_main(LV_PALETTE_RED) : lv_color_make(255, 255, 255), 0);
    } else {
      lv_obj_set_style_bg_color(ui_RPMBar, lv_color_make(34, 34, 34), LV_PART_MAIN);
    }

    if (rpm <= 3000) {
      lv_obj_set_style_bg_color(ui_RPMBar, lv_color_make(0, 180, 50), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_RPMValue, lv_color_make(255, 255, 255), 0);
    } else if (rpm > 3000 && rpm <= 5000) {
      lv_obj_set_style_bg_color(ui_RPMBar, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_RPMValue, lv_color_make(255, 255, 255), 0);
    } else if (rpm > 5000 && rpm <= 6250) {
      lv_obj_set_style_bg_color(ui_RPMBar, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
      lv_obj_set_style_text_color(ui_RPMValue, lv_color_make(255, 255, 255), 0);
    }
  }
}
void KMHReading(int kmh) {
  lv_label_set_text(ui_KMHValue, String(kmh).c_str());
}
void ENGINE_LOADReading(int load) {
  if (ui_EngineLoadArc != NULL) {
    lv_arc_set_value(ui_EngineLoadArc, load);
    lv_label_set_text_fmt(ui_EngineLoadValue2, "%d%%", load);
  }
}
void TORQUEReading(int torque) {
  if (ui_EngineTorqueArc != NULL) {
    lv_arc_set_value(ui_EngineTorqueArc, torque);
    lv_label_set_text_fmt(ui_EngineTorqueValue, "%d Nm", torque);
  }
}
void HPReading(int hp) {
  if (ui_EngineHpArc != NULL) {
    lv_arc_set_value(ui_EngineHpArc, hp);
    lv_label_set_text_fmt(ui_HPValue, "%d HP", hp);
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
  lv_obj_t *current_screen = lv_scr_act();

  if (current_screen == ui_RpmSpeedGauge) {
    currentTest = SEND_RPM;
    if (!debug) {
      switch (currentTest) {
        case SEND_RPM:
          if (millis() - last_obd_request_time < obd_interval_engine) return;

          while (client.available()) client.read();
          delay(200);

          Serial.println("\n[OBD] Richiesta RPM (010C)...");
          client.print("010C\r");
          last_attempt_time = millis();
          currentTest = READ_RPM;
          break;
        case READ_RPM:
          if (client.available()) {
            String resp = client.readStringUntil('>');
            Serial.print("[OBD] Ricevuto: ");
            Serial.println(resp);

            if (resp.indexOf("410C") != -1) {
              int index = resp.indexOf("410C");
              String hexA = resp.substring(index + 4, index + 6);
              String hexB = resp.substring(index + 6, index + 8);

              hexA.trim();
              hexB.trim();
              long A = strtol(hexA.c_str(), NULL, 16);
              long B = strtol(hexB.c_str(), NULL, 16);

              // Formula Standard OBD RPM: ((A*256)+B)/4
              int rpm = (int)((A * 256) + B) / 4;

              if (rpm >= 0 && rpm < 7000) {
                Serial.printf(">>> RPM OK: %d \n", rpm);
                RPMReading(rpm);
                consecutive_errors = 0;
                last_obd_request_time = millis();
                currentTest = SEND_KMH;
                return;
              }
            }
          } else if (millis() - last_attempt_time > 3000) {
            last_obd_request_time = millis();
            currentTest = SEND_KMH;
          }
          break;
        case SEND_KMH:
          if (millis() - last_obd_request_time < obd_interval_engine) return;

          while (client.available()) client.read();
          delay(200);

          Serial.println("\n[OBD] Richiesta KMH (010D)...");
          client.print("010D\r");
          last_attempt_time = millis();
          currentTest = READ_KMH;
          break;
        case READ_KMH:
          if (client.available()) {
            String resp = client.readStringUntil('>');
            Serial.print("[OBD] Ricevuto: ");
            Serial.println(resp);

            if (resp.indexOf("410D") != -1) {
              int index = resp.indexOf("410D");
              String hex = resp.substring(index + 4, index + 6);
              hex.trim();
              if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

              int kmh = (int)strtol(hex.c_str(), NULL, 16);

              if (kmh >= 0 && kmh < 300) {
                Serial.printf(">>> KMH OK: %d \n", kmh);
                KMHReading(kmh);
                consecutive_errors = 0;
                last_obd_request_time = millis();
                currentTest = SEND_ENGINE_LOAD;
                return;
              }
            }
          } else if (millis() - last_attempt_time > 3000) {
            last_obd_request_time = millis();
            currentTest = SEND_ENGINE_LOAD;
          }
          break;

        case SEND_ENGINE_LOAD:
          if (millis() - last_obd_request_time < obd_interval_engine) return;

          while (client.available()) client.read();
          delay(200);

          Serial.println("\n[OBD] Richiesta ENGINE_LOAD (0104)...");
          client.print("0104\r");
          last_attempt_time = millis();
          currentTest = READ_ENGINE_LOAD;
          break;
        case READ_ENGINE_LOAD:
          if (client.available()) {
            String resp = client.readStringUntil('>');
            Serial.print("[OBD] Ricevuto: ");
            Serial.println(resp);

            if (resp.indexOf("4104") != -1) {
              int index = resp.indexOf("4104");
              String hex = resp.substring(index + 4, index + 6);
              hex.trim();
              if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

              int load = ((int)strtol(hex.c_str(), NULL, 16) * 100) / 255;

              if (load >= 0 && load < 110) {
                Serial.printf(">>> ENGINE_LOAD OK: %d \n", load);
                ENGINE_LOADReading(load);
                consecutive_errors = 0;
                last_obd_request_time = millis();
                currentTest = SEND_TORQUE;
                return;
              }
            }
          } else if (millis() - last_attempt_time > 3000) {
            last_obd_request_time = millis();
            currentTest = SEND_TORQUE;
          }
          break;
        case SEND_TORQUE:
          if (millis() - last_obd_request_time < obd_interval_engine) return;

          while (client.available()) client.read();
          delay(200);

          Serial.println("\n[OBD] Richiesta TORQUE (0162)...");
          client.print("0162\r");
          last_attempt_time = millis();
          currentTest = READ_TORQUE;
          break;
        case READ_TORQUE:
          if (client.available()) {
            String resp = client.readStringUntil('>');
            Serial.print("[OBD] Ricevuto: ");
            Serial.println(resp);

            if (resp.indexOf("4162") != -1) {
              int index = resp.indexOf("4162");
              String hex = resp.substring(index + 4, index + 6);
              hex.trim();
              if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

              int torque = ((int)strtol(hex.c_str(), NULL, 16) * 245) / 255;

              if (torque >= 0 && torque < 250) {
                Serial.printf(">>> TORQUE OK: %d \n", torque);
                TORQUEReading(torque);
                consecutive_errors = 0;
                last_obd_request_time = millis();
                currentTest = SEND_HP;
                return;
              }
            }
          } else if (millis() - last_attempt_time > 3000) {
            last_obd_request_time = millis();
            currentTest = SEND_HP;
          }
          break;
        case SEND_HP:
          if (millis() - last_obd_request_time < obd_interval_engine) return;

          while (client.available()) client.read();
          delay(200);

          Serial.println("\n[OBD] Richiesta HP (maf 0110)...");
          client.print("0110\r");
          last_attempt_time = millis();
          currentTest = READ_HP;
          break;
        case READ_HP:
          if (client.available()) {
            String resp = client.readStringUntil('>');
            Serial.print("[OBD] Ricevuto: ");
            Serial.println(resp);

            if (resp.indexOf("4110") != -1) {
              int index = resp.indexOf("4110");
              String hexA = resp.substring(index + 4, index + 6);
              String hexB = resp.substring(index + 6, index + 8);

              hexA.trim();
              hexB.trim();
              long A = strtol(hexA.c_str(), NULL, 16);
              long B = strtol(hexB.c_str(), NULL, 16);

              int hp = (((A * 256) + B) / 100) / 0.81;

              if (hp >= 0 && hp < 250) {
                Serial.printf(">>> TORQUE OK: %d \n", hp);
                HPReading(hp);
                consecutive_errors = 0;
                last_obd_request_time = millis();
                currentTest = SEND_RPM;
                return;
              }
            }
          } else if (millis() - last_attempt_time > 3000) {
            last_obd_request_time = millis();
            currentTest = SEND_RPM;
          }
          break;

        case REINIT_ELM:
          Serial.println("[SYSTEM] Reset Hard...");
          client.print("ATZ\r");
          delay(2000);
          client.print("ATE0\r");
          delay(500);
          client.print("ATS0\r");
          delay(500);
          client.print("ATSP5\r");
          delay(2000);
          consecutive_errors = 0;
          currentTest = SEND_RPM;
          last_obd_request_time = millis();
          break;
      }
    } else {
      RPMReading(2000);
      KMHReading(100);
      ENGINE_LOADReading(80);
      TORQUEReading(200);
      HPReading(100);
    }
  } else if (current_screen == ui_OilWaterTemperatureGauge) {
    if (!debug) {
      currentTest = SEND_WATER;
      switch (currentTest) {
        case SEND_WATER:
          if (millis() - last_obd_request_time < obd_interval_water) return;

          while (client.available()) client.read();
          delay(200);

          Serial.println("\n[OBD] Richiesta Acqua (0105)...");
          client.print("0105\r");
          last_attempt_time = millis();
          currentTest = READ_WATER;
          break;

        case READ_WATER:
          if (client.available()) {
            String resp = client.readStringUntil('>');
            Serial.print("[OBD] Ricevuto: ");
            Serial.println(resp);

            if (resp.indexOf("4105") != -1) {
              int index = resp.indexOf("4105");
              String hex = resp.substring(index + 4, index + 6);
              hex.trim();
              if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

              int temp = (int)strtol(hex.c_str(), NULL, 16) - 40;

              if (temp > -30 && temp < 150) {
                Serial.printf(">>> ACQUA OK: %d C\n", temp);
                WaterTemperatureReading(temp);
                consecutive_errors = 0;
                last_obd_request_time = millis();
                currentTest = SEND_WATER;
                return;
              }
            }
          } else if (millis() - last_attempt_time > 3000) {
            last_obd_request_time = millis();
            currentTest = SEND_WATER;
          }
          break;

        case REINIT_ELM:
          Serial.println("[SYSTEM] Reset Hard...");
          client.print("ATZ\r");
          delay(2000);
          client.print("ATE0\r");
          delay(500);
          client.print("ATS0\r");
          delay(500);
          client.print("ATSP5\r");
          delay(2000);
          consecutive_errors = 0;
          currentTest = SEND_WATER;
          last_obd_request_time = millis();
          break;
      }
    } else {
      WaterTemperatureReading(50);
    }
  } else if (current_screen == ui_EngineManagementGauge){

  } else if (current_screen == ui_EngineHealthGauge) {
    
  }
  // if (!debug) {
  //   switch (currentTest) {
  //     case SEND_WATER:
  //       if (millis() - last_obd_request_time < obd_interval) return;

  //       while (client.available()) client.read();
  //       delay(200);

  //       Serial.println("\n[OBD] Richiesta Acqua (0105)...");
  //       client.print("0105\r");
  //       last_attempt_time = millis();
  //       currentTest = READ_WATER;
  //       break;

  //     case READ_WATER:
  //       if (client.available()) {
  //         String resp = client.readStringUntil('>');
  //         Serial.print("[OBD] Ricevuto: ");
  //         Serial.println(resp);

  //         if (resp.indexOf("4105") != -1) {
  //           int index = resp.indexOf("4105");
  //           String hex = resp.substring(index + 4, index + 6);
  //           hex.trim();
  //           if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

  //           int temp = (int)strtol(hex.c_str(), NULL, 16) - 40;

  //           if (temp > -30 && temp < 150) {
  //             Serial.printf(">>> ACQUA OK: %d C\n", temp);
  //             WaterTemperatureReading(temp);
  //             consecutive_errors = 0;
  //             last_obd_request_time = millis();
  //             currentTest = SEND_WATER;
  //             return;
  //           }
  //         }
  //       } else if (millis() - last_attempt_time > 3000) {
  //         last_obd_request_time = millis();
  //         currentTest = SEND_WATER;
  //       }
  //       break;

  //     case SEND_RPM:
  //       if (millis() - last_obd_request_time < obd_interval) return;

  //       while (client.available()) client.read();
  //       delay(200);

  //       Serial.println("\n[OBD] Richiesta RPM (010C)...");
  //       client.print("010C\r");
  //       last_attempt_time = millis();
  //       currentTest = READ_RPM;
  //       break;
  //     case READ_RPM:
  //       if (client.available()) {
  //         String resp = client.readStringUntil('>');
  //         Serial.print("[OBD] Ricevuto: ");
  //         Serial.println(resp);

  //         if (resp.indexOf("410C") != -1) {
  //           int index = resp.indexOf("410C");
  //           String hexA = resp.substring(index + 4, index + 6);
  //           String hexB = resp.substring(index + 6, index + 8);

  //           hexA.trim();
  //           hexB.trim();
  //           long A = strtol(hexA.c_str(), NULL, 16);
  //           long B = strtol(hexB.c_str(), NULL, 16);

  //           // Formula Standard OBD RPM: ((A*256)+B)/4
  //           int rpm = (int)((A * 256) + B) / 4;

  //           if (rpm >= 0 && rpm < 7000) {
  //             Serial.printf(">>> RPM OK: %d \n", rpm);
  //             RPMReading(rpm);
  //             consecutive_errors = 0;
  //             last_obd_request_time = millis();
  //             currentTest = SEND_KMH;
  //             return;
  //           }
  //         }
  //       } else if (millis() - last_attempt_time > 3000) {
  //         last_obd_request_time = millis();
  //         currentTest = SEND_KMH;
  //       }
  //       break;
  //     case SEND_KMH:
  //       if (millis() - last_obd_request_time < obd_interval) return;

  //       while (client.available()) client.read();
  //       delay(200);

  //       Serial.println("\n[OBD] Richiesta KMH (010D)...");
  //       client.print("010D\r");
  //       last_attempt_time = millis();
  //       currentTest = READ_KMH;
  //       break;
  //     case READ_KMH:
  //       if (client.available()) {
  //         String resp = client.readStringUntil('>');
  //         Serial.print("[OBD] Ricevuto: ");
  //         Serial.println(resp);

  //         if (resp.indexOf("410D") != -1) {
  //           int index = resp.indexOf("410D");
  //           String hex = resp.substring(index + 4, index + 6);
  //           hex.trim();
  //           if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

  //           int kmh = (int)strtol(hex.c_str(), NULL, 16);

  //           if (kmh >= 0 && kmh < 300) {
  //             Serial.printf(">>> KMH OK: %d \n", kmh);
  //             KMHReading(kmh);
  //             consecutive_errors = 0;
  //             last_obd_request_time = millis();
  //             currentTest = SEND_ENGINE_LOAD;
  //             return;
  //           }
  //         }
  //       } else if (millis() - last_attempt_time > 3000) {
  //         last_obd_request_time = millis();
  //         currentTest = SEND_ENGINE_LOAD;
  //       }
  //       break;

  //     case SEND_ENGINE_LOAD:
  //       if (millis() - last_obd_request_time < obd_interval) return;

  //       while (client.available()) client.read();
  //       delay(200);

  //       Serial.println("\n[OBD] Richiesta ENGINE_LOAD (0104)...");
  //       client.print("0104\r");
  //       last_attempt_time = millis();
  //       currentTest = READ_ENGINE_LOAD;
  //       break;
  //     case READ_ENGINE_LOAD:
  //       if (client.available()) {
  //         String resp = client.readStringUntil('>');
  //         Serial.print("[OBD] Ricevuto: ");
  //         Serial.println(resp);

  //         if (resp.indexOf("4104") != -1) {
  //           int index = resp.indexOf("4104");
  //           String hex = resp.substring(index + 4, index + 6);
  //           hex.trim();
  //           if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

  //           int load = ((int)strtol(hex.c_str(), NULL, 16) * 100) / 255;

  //           if (load >= 0 && load < 110) {
  //             Serial.printf(">>> ENGINE_LOAD OK: %d \n", load);
  //             ENGINE_LOADReading(load);
  //             consecutive_errors = 0;
  //             last_obd_request_time = millis();
  //             currentTest = SEND_TORQUE;
  //             return;
  //           }
  //         }
  //       } else if (millis() - last_attempt_time > 3000) {
  //         last_obd_request_time = millis();
  //         currentTest = SEND_TORQUE;
  //       }
  //       break;
  //     case SEND_TORQUE:
  //       if (millis() - last_obd_request_time < obd_interval) return;

  //       while (client.available()) client.read();
  //       delay(200);

  //       Serial.println("\n[OBD] Richiesta TORQUE (0162)...");
  //       client.print("0162\r");
  //       last_attempt_time = millis();
  //       currentTest = READ_TORQUE;
  //       break;
  //     case READ_TORQUE:
  //       if (client.available()) {
  //         String resp = client.readStringUntil('>');
  //         Serial.print("[OBD] Ricevuto: ");
  //         Serial.println(resp);

  //         if (resp.indexOf("4162") != -1) {
  //           int index = resp.indexOf("4162");
  //           String hex = resp.substring(index + 4, index + 6);
  //           hex.trim();
  //           if (hex.length() < 2) hex = resp.substring(index + 5, index + 7);

  //           int torque = ((int)strtol(hex.c_str(), NULL, 16) * 245) / 255;

  //           if (torque >= 0 && torque < 250) {
  //             Serial.printf(">>> TORQUE OK: %d \n", torque);
  //             TORQUEReading(torque);
  //             consecutive_errors = 0;
  //             last_obd_request_time = millis();
  //             currentTest = SEND_HP;
  //             return;
  //           }
  //         }
  //       } else if (millis() - last_attempt_time > 3000) {
  //         last_obd_request_time = millis();
  //         currentTest = SEND_HP;
  //       }
  //       break;
  //     case SEND_HP:
  //       if (millis() - last_obd_request_time < obd_interval) return;

  //       while (client.available()) client.read();
  //       delay(200);

  //       Serial.println("\n[OBD] Richiesta HP (maf 0110)...");
  //       client.print("0110\r");
  //       last_attempt_time = millis();
  //       currentTest = READ_HP;
  //       break;
  //     case READ_HP:
  //       if (client.available()) {
  //         String resp = client.readStringUntil('>');
  //         Serial.print("[OBD] Ricevuto: ");
  //         Serial.println(resp);

  //         if (resp.indexOf("4110") != -1) {
  //           int index = resp.indexOf("4110");
  //           String hexA = resp.substring(index + 4, index + 6);
  //           String hexB = resp.substring(index + 6, index + 8);

  //           hexA.trim();
  //           hexB.trim();
  //           long A = strtol(hexA.c_str(), NULL, 16);
  //           long B = strtol(hexB.c_str(), NULL, 16);

  //           int hp = (((A * 256) + B) / 100) / 0.81;

  //           if (hp >= 0 && hp < 250) {
  //             Serial.printf(">>> TORQUE OK: %d \n", hp);
  //             HPReading(hp);
  //             consecutive_errors = 0;
  //             last_obd_request_time = millis();
  //             currentTest = SEND_WATER;
  //             return;
  //           }
  //         }
  //       } else if (millis() - last_attempt_time > 3000) {
  //         last_obd_request_time = millis();
  //         currentTest = SEND_WATER;
  //       }
  //       break;

  //     case REINIT_ELM:
  //       Serial.println("[SYSTEM] Reset Hard...");
  //       client.print("ATZ\r");
  //       delay(2000);
  //       client.print("ATE0\r");
  //       delay(500);
  //       client.print("ATS0\r");
  //       delay(500);
  //       client.print("ATSP5\r");
  //       delay(2000);
  //       consecutive_errors = 0;
  //       currentTest = SEND_WATER;
  //       last_obd_request_time = millis();
  //       break;
  //   }
  // } else {
  //   WaterTemperatureReading(50);
  //   RPMReading(2000);
  //   KMHReading(100);
  //   ENGINE_LOADReading(80);
  //   TORQUEReading(200);
  //   HPReading(100);
  // }
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

  if (!debug) {
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
      if (!myELM327.begin(client, true, 2000)) {
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

    Serial.println("setup ELM completato!");
    reinit_sequence();
  }
  lv_scr_load_anim(ui_RpmSpeedGauge, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
  unsigned long start_time = millis();
  while (millis() - start_time < 500) {  // Ciclo di mezzo secondo
    lv_timer_handler();
    delay(5);
  }
}

void loop() {
  lv_timer_handler();
  if (!debug) {
    if (WiFi.status() == WL_CONNECTED && client.connected()) {
      test_obd_readings();
    }
  } else {
    test_obd_readings();
  }
  delay(5);
}