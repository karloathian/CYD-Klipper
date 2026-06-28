#include "battery_monitor.hpp"
#include "driver/i2c.h"
#include "lvgl.h"
#include "printer_integration.hpp"

#ifdef BOARD_HAS_IP5306

// Determine I2C port from board configuration
#ifdef GT911_I2C_HOST
#define BATTERY_I2C_PORT GT911_I2C_HOST
#else
#define BATTERY_I2C_PORT I2C_NUM_0
#endif

static bool is_available = false;
static uint8_t last_percent = 100;
static lv_timer_t *battery_timer = nullptr;

// Helper to probe for the IP5306 (address 0x75) using ESP-IDF I2C driver
static bool probe_ip5306()
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    // Write address 0x75 (write bit)
    i2c_master_write_byte(cmd, (0x75 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    // Send command with a 50ms timeout
    esp_err_t ret = i2c_master_cmd_begin(BATTERY_I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        Serial.printf("[Battery] IP5306 found at address 0x75 on I2C port %d\n", BATTERY_I2C_PORT);
        return true;
    } else {
        Serial.printf("[Battery] Probe at 0x75 failed: %s (0x%X) on port %d\n", esp_err_to_name(ret), ret, BATTERY_I2C_PORT);
    }
    return false;
}

// Read register from the IP5306 using ESP-IDF I2C driver
static bool read_ip5306_reg(uint8_t reg, uint8_t *val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x75 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x75 << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(BATTERY_I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK);
}

static void read_ip5306_battery()
{
    if (!is_available) return;

    uint8_t reg_val = 0;
    if (read_ip5306_reg(0x78, &reg_val)) {
        // IP5306 reports charge level in the high bits of register 0x78:
        // 0xE0 = 100%, 0xC0 = 75%, 0x80 = 50%, 0x00 = 25% (or lower)
        uint8_t raw_lvl = reg_val & 0xE0;
        uint8_t new_pct = 25;
        
        if (raw_lvl == 0xE0) {
            new_pct = 100;
        } else if (raw_lvl == 0xC0) {
            new_pct = 75;
        } else if (raw_lvl == 0x80) {
            new_pct = 50;
        } else {
            new_pct = 25;
        }
        
        if (new_pct != last_percent) {
            last_percent = new_pct;
            Serial.printf("[Battery] Level updated: %d%%\n", last_percent);
        }
        
        // Always dispatch the message so widgets can initialize and refresh
        lv_msg_send(DATA_BATTERY, &last_percent);
    } else {
        Serial.println("[Battery] Failed to read battery level from IP5306");
    }
}

static void battery_timer_cb(lv_timer_t *timer)
{
    read_ip5306_battery();
}

void battery_monitor_init()
{
    // Probe using already initialized ESP-IDF I2C driver
    if (probe_ip5306()) {
        is_available = true;
        read_ip5306_battery();
        
        // Schedule periodic reading every 30 seconds
        battery_timer = lv_timer_create(battery_timer_cb, 30000, nullptr);
    } else {
        Serial.println("[Battery] IP5306 PMIC not detected on I2C bus. Battery monitoring disabled.");
    }
}

uint8_t get_battery_percent()
{
    return last_percent;
}

bool is_battery_monitor_available()
{
    return is_available;
}

#else

// Fallback empty implementation for other boards
void battery_monitor_init() {}
uint8_t get_battery_percent() { return 100; }
bool is_battery_monitor_available() { return false; }

#endif // BOARD_HAS_IP5306
