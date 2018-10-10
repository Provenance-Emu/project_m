// Simple test of USB Host Joystick
//
// This example is in the public domain
// 
// Notes: 
//Remname the device, http://liveelectronics.musinou.net/MIDIdeviceName.php
// https://github.com/arduino-libraries/MIDIUSB/issues/37
// https://github.com/PaulStoffregen/USBHost_t36
// https://github.com/FortySevenEffects/arduino_midi_library
// http://playground.arduino.cc/Code/Bounce
// https://playground.arduino.cc/Main/SketchList#Math_code
// Displays
// Red
// Maybe? https://github.com/Nkawu/TFT_22_ILI9225
// https://www.pjrc.com/store/display_ili9341.html
// optimized https://github.com/PaulStoffregen/ILI9341_t3
// DMA optimized https://github.com/KurtE/ILI9341_t3n
// https://www.arduino.cc/en/Reference/TFTLibrary
// USB Bluetooth: https://github.com/KurtE/USBHost_t36/tree/Bluetooth-WIP
// SD card: https://www.arduino.cc/en/Reference/SD
// MIDI for Teensy docs, https://www.pjrc.com/teensy/td_midi.html

//
// #include <Arduino.h>
#include "TeensyThreads.h"
#include "USBHost_t36.h"
#include <MIDI.h>
#include <sstream>
#include <stdio.h>
#include "MIDIUSB.h"

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
#define COUNT_JOYSTICKS 4

const int LED = 13;

JoystickController joysticks[COUNT_JOYSTICKS](myusb);
uint32_t buttons_prev[COUNT_JOYSTICKS] = {0, 0, 0, 0};

int user_axis[64];

USBDriver *drivers[] = {&hub1, &joysticks[0], &joysticks[1], &joysticks[2], &joysticks[3], &hid1, &hid2, &hid3, &hid4};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "joystick[0D]", "joystick[1D]", "joystick[2D]", "joystick[3D]",  "HID1", "HID2", "HID3", "HID4"};
bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&joysticks[0], &joysticks[1], &joysticks[2], &joysticks[3]};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"joystick[0H]", "joystick[1H]", "joystick[2H]", "joystick[3H]"};
bool hid_driver_active[CNT_DEVICES] = {false};
bool show_changed_only = true;

uint8_t joystick_left_trigger_value[COUNT_JOYSTICKS] = {0};
uint8_t joystick_right_trigger_value[COUNT_JOYSTICKS] = {0};
uint64_t joystick_full_notify_mask = (uint64_t) - 1;

//=============================================================================
// Setup
//=============================================================================
void setup()
{
 // initialize the digital pin as an output.
//  pinMode(led, OUTPUT);     
  
  while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.println("\n\nUSB Host Joystick Testing");
  myusb.begin();

  pinMode(LED, OUTPUT);
  threads.addThread(ledThread);
  led(true);
}


//=============================================================================
// loop
//=============================================================================
void loop()
{
  myusb.Task();
  PrintDeviceListChanges();

  if (Serial.available()) {
    int ch = Serial.read(); // get the first char.
    while (Serial.read() != -1) ;
    if ((ch == 'b') || (ch == 'B')) {
      Serial.println("Only notify on Basic Axis changes");
      for (int joystick_index = 0; joystick_index < COUNT_JOYSTICKS; joystick_index++)
        joysticks[joystick_index].axisChangeNotifyMask(0x3ff);
    } else if ((ch == 'f') || (ch == 'F')) {
      Serial.println("Only notify on Full Axis changes");
      for (int joystick_index = 0; joystick_index < COUNT_JOYSTICKS; joystick_index++)
        joysticks[joystick_index].axisChangeNotifyMask(joystick_full_notify_mask);

    } else {
      if (show_changed_only) {
        show_changed_only = false;
        Serial.println("\n*** Show All fields mode ***");
      } else {
        show_changed_only = true;
        Serial.println("\n*** Show only changed fields mode ***");
      }
    }
  }

  for (int joystick_index = 0; joystick_index < COUNT_JOYSTICKS; joystick_index++) {
    if (joysticks[joystick_index].available()) {
      uint64_t axis_mask = joysticks[joystick_index].axisMask();
      uint64_t axis_changed_mask = joysticks[joystick_index].axisChangedMask();
      uint32_t buttons = joysticks[joystick_index].getButtons();

      led(true);
      
      Serial.printf("Joystick(%d): buttons = %x", joystick_index, buttons);
      //Serial.printf(" AMasks: %x %x:%x", axis_mask, (uint32_t)(user_axis_mask >> 32), (uint32_t)(user_axis_mask & 0xffffffff));
      //Serial.printf(" M: %lx %lx", axis_mask, joysticks[joystick_index].axisChangedMask());
      if (show_changed_only) {
        for (uint8_t i = 0; axis_changed_mask != 0; i++, axis_changed_mask >>= 1) {
          if (axis_changed_mask & 1) {

            int ccNumber = i + 64;
            int value = joysticks[joystick_index].getAxis(i);
            int channel = joystick_index + 1;
            usbMIDI.sendControlChange(ccNumber, value, channel);
// TODO: NRPN for joysticks
            Serial.printf("Changed Axis %d:%d", i, value);
          }
        }

      } else {
        for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
          if (axis_mask & 1) {
            Serial.printf("Axis %d:%d", i, joysticks[joystick_index].getAxis(i));
          }
        }
      }
      uint8_t ltv;
      uint8_t rtv;
      switch (joysticks[joystick_index].joystickType) {
        default:
          break;
        case JoystickController::PS4:
          ltv = joysticks[joystick_index].getAxis(3);
          rtv = joysticks[joystick_index].getAxis(4);
          if ((ltv != joystick_left_trigger_value[joystick_index]) || (rtv != joystick_right_trigger_value[joystick_index])) {
            joystick_left_trigger_value[joystick_index] = ltv;
            joystick_right_trigger_value[joystick_index] = rtv;
            joysticks[joystick_index].setRumble(ltv, rtv);
          }
          break;

        case JoystickController::PS3:
          ltv = joysticks[joystick_index].getAxis(18);
          rtv = joysticks[joystick_index].getAxis(19);
          if ((ltv != joystick_left_trigger_value[joystick_index]) || (rtv != joystick_right_trigger_value[joystick_index])) {
            joystick_left_trigger_value[joystick_index] = ltv;
            joystick_right_trigger_value[joystick_index] = rtv;
            joysticks[joystick_index].setRumble(ltv, rtv, 50);
          }
          break;

        case JoystickController::XBOXONE:
        case JoystickController::XBOX360:
          ltv = joysticks[joystick_index].getAxis(4);
          rtv = joysticks[joystick_index].getAxis(5);
          if ((ltv != joystick_left_trigger_value[joystick_index]) || (rtv != joystick_right_trigger_value[joystick_index])) {
            joystick_left_trigger_value[joystick_index] = ltv;
            joystick_right_trigger_value[joystick_index] = rtv;
            joysticks[joystick_index].setRumble(ltv, rtv);
            Serial.printf(" Set Rumble %d %d", ltv, rtv);
          }
          break;
      }

      uint8_t lbuttons_prev = buttons_prev[joystick_index];
      if (buttons != lbuttons_prev) {
        if (joysticks[joystick_index].joystickType == JoystickController::PS3) {
          joysticks[joystick_index].setLEDs((buttons >> 12) & 0xf); //  try to get to TRI/CIR/X/SQuare
        } else {
          uint8_t lr = (buttons & 1) ? 0xff : 0;
          uint8_t lg = (buttons & 2) ? 0xff : 0;
          uint8_t lb = (buttons & 4) ? 0xff : 0;
          joysticks[joystick_index].setLEDs(lr, lg, lb);
        }

        uint32_t mask = buttons ^ lbuttons_prev;
        int place = 0;
        while (mask > 0) {
          mask >>= 1;
          place++;
        }

        bool on = buttons && (0x01 << place);
        char value = on ? 0xFF : 0x00;
        int channel = joystick_index + 1;
        int ccNumber = 70 + place;

        Serial.printf("\nCC: %i Value: 0x%X Channel: %i", ccNumber, value, channel);
        usbMIDI.sendControlChange(ccNumber, value, channel);

        buttons_prev[joystick_index] = buttons;
      }

      Serial.println();
      joysticks[joystick_index].joystickDataClear();
    }
  }
}

// ----- LED


// constants won't change:
const long interval = 100;           // interval at which to blink (milliseconds)
// Variables will change:
bool ledState = false;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

void led(bool on) {
  previousMillis = millis();
  ledState = on;
}

void ledThread() {
 while(1) {
  unsigned long currentMillis = millis();

  if(ledState && currentMillis - previousMillis <= interval) {
    digitalWrite(LED, HIGH);
  } else {
    digitalWrite(LED, LOW);
  }

   threads.yield();
 }
}

//=============================================================================
// Show when devices are added or removed
//=============================================================================
void PrintDeviceListChanges() {
  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;

        byte stringBuffer[255];
        byte sysexBuffer[255];
        memset(stringBuffer, 0, 255 * sizeof(byte));
        memset(sysexBuffer, 0, 255 * sizeof(byte));
        int n;
        n=sprintf ((char*)stringBuffer, "{disconnect : %i}", i);

        const unsigned encodedSize = midi::encodeSysEx(stringBuffer, sysexBuffer, n);
        usbMIDI.sendSysEx(encodedSize, sysexBuffer, false);

      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *man = drivers[i]->manufacturer();
        const uint8_t *prod = drivers[i]->product();
        const uint8_t *ser = drivers[i]->serialNumber();

        if (man && *man) Serial.printf("  manufacturer: %s\n", man);
        if (prod && *prod) Serial.printf("  product: %s\n", prod);
        if (ser && *ser) Serial.printf("  Serial: %s\n", ser);

        byte stringBuffer[255];
        byte sysexBuffer[255];
        memset(stringBuffer, 0, 255 * sizeof(byte));
        memset(sysexBuffer, 0, 255 * sizeof(byte));
        int n;
        n=sprintf ((char*)stringBuffer, "{connect : {index: %i, manuf : \"%s\", prod : \"%s\", serial : \"%s\"}}", i, man, prod, ser);

        const unsigned encodedSize = midi::encodeSysEx(stringBuffer, sysexBuffer, n);
        usbMIDI.sendSysEx(encodedSize, sysexBuffer, false);
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
      }
    }
  }
}

void mySystemExclusive(byte *data, unsigned int length) {
  Serial.print("SysEx Message: ");
  printBytes(data, length);
  Serial.println();
}

void printBytes(const byte *data, unsigned int size) {
  while (size > 0) {
    byte b = *data++;
    if (b < 16) Serial.print('0');
    Serial.print(b, HEX);
    if (size > 1) Serial.print(' ');
    size = size - 1;
  }
}
