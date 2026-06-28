#include "led_status.hpp"
#include <Arduino.h>
#include "lvgl.h"
#include "printer_integration.hpp"

#ifdef BOARD_HAS_RGB_LED

// Define pins with fallbacks if they are not defined
#ifndef RGB_LED_R
#define RGB_LED_R 4
#endif
#ifndef RGB_LED_G
#define RGB_LED_G 16
#endif
#ifndef RGB_LED_B
#define RGB_LED_B 17
#endif

// State variables
static PrinterState current_printer_state = PrinterStateOffline;
static bool is_battery_low = false;
static lv_timer_t *led_blink_timer = nullptr;
static bool blink_toggle = false;

// Set physical LED colors (active-LOW, so we invert the value)
static void set_physical_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    analogWrite(RGB_LED_R, 255 - r);
    analogWrite(RGB_LED_G, 255 - g);
    analogWrite(RGB_LED_B, 255 - b);
}

// Update the LED color based on current printer and battery states
static void led_update()
{
    // If there's an active error, blink fast red (handled by timer, called separately)
    if (current_printer_state == PrinterStateError) {
        if (blink_toggle) {
            set_physical_led_color(255, 0, 0); // Bright Red
        } else {
            set_physical_led_color(0, 0, 0);   // Off
        }
        return;
    }

    // Battery low alarm overrides normal idle/printing states
    if (is_battery_low) {
        set_physical_led_color(40, 20, 0); // Dim Yellow/Orange warning
        return;
    }

    // Map printer states to solid colors
    switch (current_printer_state) {
        case PrinterStateOffline:
            set_physical_led_color(0, 0, 0);   // Off
            break;
        case PrinterStateIdle:
            set_physical_led_color(0, 20, 0);  // Dim Green
            break;
        case PrinterStatePrinting:
            set_physical_led_color(0, 0, 20);  // Dim Blue
            break;
        case PrinterStatePaused:
            set_physical_led_color(255, 0, 0); // Solid Red (Warning/Intervention needed)
            break;
        default:
            set_physical_led_color(0, 0, 0);   // Off
            break;
    }
}

// Periodic timer for blinking animation
static void led_blink_timer_cb(lv_timer_t *timer)
{
    if (current_printer_state == PrinterStateError) {
        blink_toggle = !blink_toggle;
        led_update();
    }
}

// Handle printer state changes
static void on_led_state_change(void *s, lv_msg_t *m)
{
    PrinterData* printer = get_current_printer_data();
    if (printer) {
        current_printer_state = printer->state;
        led_update();
    }
}

// Handle battery state changes (low battery threshold)
static void on_led_battery_change(void *s, lv_msg_t *m)
{
    const uint8_t *pct_ptr = (const uint8_t *)lv_msg_get_payload(m);
    if (pct_ptr) {
        uint8_t pct = *pct_ptr;
        // Mark battery as low if it is below 25%
        is_battery_low = (pct > 0 && pct <= 25);
        led_update();
    }
}

void led_status_init()
{
    // Configure pin modes
    pinMode(RGB_LED_R, OUTPUT);
    pinMode(RGB_LED_G, OUTPUT);
    pinMode(RGB_LED_B, OUTPUT);

    // Initial state: Off
    set_physical_led_color(0, 0, 0);

    // Setup LVGL msg subscriptions
    lv_msg_subscribe(DATA_PRINTER_STATE, on_led_state_change, nullptr);
    
    // Subscribe to battery status message (DATA_BATTERY = 6)
    lv_msg_subscribe(6, on_led_battery_change, nullptr);

    // Setup periodic blink timer (500ms intervals)
    led_blink_timer = lv_timer_create(led_blink_timer_cb, 500, nullptr);

    // Apply current state
    PrinterData* printer = get_current_printer_data();
    if (printer) {
        current_printer_state = printer->state;
        led_update();
    }
}

#else

// Fallback empty implementation for boards without an RGB LED
void led_status_init() {}

#endif // BOARD_HAS_RGB_LED
