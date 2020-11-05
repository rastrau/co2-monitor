/*

  Original code (overall) by Hochschule Trier, Umwelt-Campus
  Birkenfeld, co2ampel.org

  Original code for Sensirion SCD30 sensor by Nathan Seidle,
  SparkFun Electronics, and additional contributors
  https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library

  Original code for LCD 2013 Copyright (c) Seeed Technology
  Inc., Author:Loovee

  Some further development by Ralph Straumann

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/


#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_IS31FL3731.h>
#include <rgb_lcd.h>
#include <math.h>

float min_value = 500;   // Minimum value for the trend bar chart
float max_value = 1200;	 // Maximum value for the trend bar chart
float refresh_rate = 30; // Display refresh rate in seconds, >= 2
int elevation_asl = 415; // Elevation above sea-level in meters

// Offset of temperature in degrees Celsius, i.e. how much 
// is the sensor being heated by its environment, e.g. other 
// electronic components?
// This parameter cannot be < 0, cf. 
// https://github.com/paulvha/scd30/blob/master/src/paulvha_SCD30.cpp
float temperature_offset = 2.5;


// Initialize array to store historical CO2 data
#define array_len 100
float ArrayData[array_len];

// Initialize Sensirion SCD30 sensor
SCD30 airSensorSCD30;

// Initialize Adafruit Charlieplex LED Matrix
Adafruit_IS31FL3731_Wing matrix = Adafruit_IS31FL3731_Wing();
int mx_height = matrix.height();

// Initialize Seeedstudio Grove 16x2 LCD
rgb_lcd lcd;

// Draw bar chart of most recent CO2 values
void drawArrayOnMatrix() {
  Serial.println("\nBar chart:");
  for (int i = 0; i < 15; i++) {
    matrix.drawLine(i, 0, i, mx_height - 1, 0); // clear the bar at index i
    float value = ArrayData[i]; // get value at index i from array of historical data

    // Modulate brightness based on value (quadratically)
    float brightness = pow((value - min_value) / (max_value - min_value), 3) * 255; //
    brightness = fmin(fmax(5, brightness), 255); // clamp brightness to [5, 255]
    // Do not draw bars (or rather: pixels) when the data is 0 (this should only 
    // occur, when the device is restarted and the data array empty)
    if (value == 0) {
      brightness = 0;
    }
    
    // Calculate height of bar based on value (linearly)
    float bar_height_raw = round((value - min_value) / (max_value - min_value) * mx_height);
    float bar_height = fmin(fmax(1, bar_height_raw), mx_height); // clamp bar height to [1, mx_height]

    Serial.println("  Index " + String(i) + ": " + String(value) + " CO2, " + String(bar_height) + " bar height, " + String(brightness) + " brightness");

    // Draw bar
    matrix.drawLine(i, mx_height - 1, i, mx_height - bar_height, brightness);
  }
}



void setup() {
  Serial.begin(115200);

  // Initialize I2C bus
  Wire.begin();

  // Check I2C bus and sensor. If there is a problem print to Serial.
  if (Wire.status() != I2C_OK) {
    Serial.println("Something is wrong with I2C bus.");
  }

  if (airSensorSCD30.begin() == false) {
    Serial.println("The SCD30 sensor did not respond. Please check wiring.");
    while (1) {
      yield();
      delay(1);
    }
  }

  // Disable auto-calibration for Sensirion SCD30
  // Explanation: For auto-calibration to work properly, SCD30
  // has to experience fresh air on a regular basis. Optimal
  // working conditions are given when the sensor is exposed to
  // fresh air for one hour every day. This is not realistic for
  // most usage contexts of the device.
  // https://cdn.sparkfun.com/assets/d/c/0/7/2/SCD30_Interface_Description.pdf
  airSensorSCD30.setAutoSelfCalibration(false);
  Serial.println("Auto-calibration of SCD30 sensor deactivated.");
  airSensorSCD30.setAltitudeCompensation(elevation_asl);
  
  // Set temperature offset (compensation)
  airSensorSCD30.setTemperatureOffset(temperature_offset); 

  // Set measurement interval in seconds
  airSensorSCD30.setMeasurementInterval(5);

  matrix.begin();
  delay(10);
  matrix.clear();

  lcd.begin(16, 2);

  Wire.setClock(100000L);             // 100 kHz SCD30
  Wire.setClockStretchLimit(200000L); // CO2 SCD30
}


void loop() {
  // Shift the data in the data array 'to the left'
  Serial.println("\nUpdating data array...");
  for (int i = 1; i < 15; i++) {
    ArrayData[i - 1] = ArrayData[i];
    yield();
  }

  // Read CO2 value from SCD30 and insert into the data array at 'most recent' position
  float co2 = airSensorSCD30.getCO2();
  ArrayData[15 - 1] = co2;
  
  // Draw the contents of the data array using the LED matrix
  drawArrayOnMatrix();

  // Write the current CO2 value to the LCD
  lcd.setCursor(0, 0);
  lcd.print(String(int(co2)) + " ppm CO2  ");

  // Write the current temperature to the LCD (2nd line, left-aligned)
  lcd.setCursor(0, 1);
  lcd.printf("%.1f", airSensorSCD30.getTemperature() - 2.5);
  lcd.setCursor(5, 1);
  lcd.printf("deg");

  // Write the current relative humidity to the LCD
  // (2nd line, right-aligned)
  lcd.setCursor(11, 1);
  lcd.print(String(int(round(airSensorSCD30.getHumidity()))) + " %h");
  
  delay(refresh_rate * 1000);
}
