#pragma once
#include <Arduino.h>

// Initialize battery monitor, probe I2C for IP5306
void battery_monitor_init();

// Returns the last read battery percentage (0, 25, 50, 75, or 100)
uint8_t get_battery_percent();

// Returns true if the battery monitor (IP5306) was successfully found on the I2C bus
bool is_battery_monitor_available();
