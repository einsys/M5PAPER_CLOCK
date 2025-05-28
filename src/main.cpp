#include <M5EPD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"
#include <nvs.h>
#include "nanum.h"
#include "hack.h"
#include "mono_mini.h"


// Settings
int8_t global_timezone = 9; // 0 = UK, 9 = KOR/JPN
const char *wifi_ssid = "YOUR_WIFI_AP";
const char *wifi_password = "YOUR_WIFI_PASSWORD";
const char *ntpServer = "time.cloudflare.com";          // Change if needed


// Preferences
const uint WIFI_CONNECT_TIMEOUT_LIMIT = 10000;                  // in milliseconds
const int16_t TEMP_CHANGE_THRESHOLD = 1;        // 0.1 deg. C


// Positions
const int16_t POS_TEMP_CURRENT_X = 0;
const int16_t POS_TEMP_CURRENT_Y = 0;
// const int16_t POS_TEMP_CURRENT_W = 140;
// const int16_t POS_TEMP_CURRENT_H = 40;

const int16_t POS_TEMP_MINMAX_X = 140;
const int16_t POS_TEMP_MINMAX_Y = 0;
// const int16_t POS_TEMP_MINMAX_W = 420;
// const int16_t POS_TEMP_MINMAX_H = 40;

const int16_t POS_VOLTAGE_X = 820;
const int16_t POS_VOLTAGE_Y = 0;
// const int16_t POS_VOLTAGE_W = 140;
// const int16_t POS_VOLTAGE_H = 40;

const int16_t POS_TIME_X = 3;
const int16_t POS_TIME_Y = 110;
// const int16_t POS_TIME_W = 954;
// const int16_t POS_TIME_H = 249;

const int16_t POS_DATE_X = 4;
const int16_t POS_DATE_Y = 440;
// const int16_t POS_DATE_W = 952;
// const int16_t POS_DATE_H = 100;


// Other vars
const int16_t SCREEN_WIDTH = 960;
const int16_t SCREEN_HEIGHT = 540;

int16_t last_temp = 0, low_temp = 999, high_temp = -999;
int16_t forced_shutdown = 0;

rtc_time_t current_time;

// Only for external power source (not recorded in NVR)
float last_hum = -1;
float last_tem = -1;        // different from last_temp, which is already declared in original code and being recorded in NVR
float last_voltage = -1;
int8_t last_hr = -1;
int8_t last_min = -1;
int8_t last_day = -1;
bool refreshed = false;

// Note that only currently required characters exist in hack.h (e.g. characters like "a" or "B" don't exist)
const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

// NVR-related; uncomment if needed
// =============================
//int nvr_save = 0;
int cleared = 0;

// void load_persistent_data(void) {
//     // Load our 3 state variables from NVS
//     nvs_handle nvs_arg;
//     nvs_open("M5-Clock", NVS_READONLY, &nvs_arg);
//     nvs_get_i16(nvs_arg, "last_temp", &last_temp);
//     nvs_get_i16(nvs_arg, "low_temp", &low_temp);
//     nvs_get_i16(nvs_arg, "high_temp", &high_temp);
//     nvs_get_i16(nvs_arg, "forced_shutdown", &forced_shutdown);
//     nvs_close(nvs_arg);
// }
// =============================


void setup() {
    M5.begin(false, false, true, true, false);

    M5.SHT30.Begin();
    M5.RTC.begin();
    M5.RTC.getTime(&current_time);
    M5.EPD.SetRotation(0);
    M5.TP.SetRotation(0);
    
    // NVR
    // load_persistent_data();

    // These lines should NOT be here if you're using internal battery.
    M5.EPD.Clear(true);
    sync();
    // ======================
    
    // These lines should be uncommented when using battery.
    // if ((current_time.min == 0) || forced_shutdown) {
    //     // Clear the display
    //     M5.EPD.Clear(true);
    //     cleared = 1;

    //     if (forced_shutdown) {
    //         forced_shutdown = 0;
    //         nvr_save = 1;     
    //     }
    // }
}


// NVR
// void save_persistent_data(void) {
//     // Save our 3 state variables - used sparingly to avoid wear on the flash
//     nvs_handle nvs_arg;
//     nvs_open("M5-Clock", NVS_READWRITE, &nvs_arg);
//     nvs_set_i16(nvs_arg, "last_temp", last_temp);
//     nvs_set_i16(nvs_arg, "low_temp", low_temp);
//     nvs_set_i16(nvs_arg, "high_temp", high_temp);
//     nvs_set_i16(nvs_arg, "forced_shutdown", forced_shutdown);
//     nvs_close(nvs_arg);
//     delay(50);
// }


int dayofweek(int y, int m, int d) {
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}


bool is_leap_year(int y) {
    return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}


int iso_week_number(int year, int month, int day) {
    bool isLeapYear = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));

    int dow = dayofweek(year, month, day);
    dow = (dow == 0) ? 7 : dow;

    int K = (month <= 2) ? 0 : (isLeapYear ? -1 : -2);
    int doy = (275 * month) / 9 - K + day - 30;
    int woy = (doy - dow + 10) / 7;

    if (woy < 1) {
        return iso_week_number(year - 1, 12, 31);
    } else if (woy > 52) {
        int jan1_dow = dayofweek(year, 1, 1);
        jan1_dow = (jan1_dow == 0) ? 7 : jan1_dow;
        if ((jan1_dow == 4) || (isLeapYear && jan1_dow == 3)) return 53;
        else return 1;
    }

    return woy;
}


void sync() {
    WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.waitForConnectResult(WIFI_CONNECT_TIMEOUT_LIMIT) != WL_CONNECTED) return;

    configTime(global_timezone * 3600, 0, ntpServer);
    struct tm timeInfo;

    if (getLocalTime(&timeInfo)) {
        rtc_time_t time_struct;
        time_struct.hour = timeInfo.tm_hour;
        time_struct.min = timeInfo.tm_min;
        time_struct.sec = timeInfo.tm_sec;
        M5.RTC.setTime(&time_struct);
        rtc_date_t date_struct;
        date_struct.week = timeInfo.tm_wday;
        date_struct.mon = timeInfo.tm_mon + 1;
        date_struct.day = timeInfo.tm_mday;
        date_struct.year = timeInfo.tm_year + 1900;
        M5.RTC.setDate(&date_struct);
        return;
    }
}

void loop() {
    char sensorString[25];
    char voltageString[5];
    char timeString[5];
    char dateString[14];

    // Canvas creation: takes 18ms
    M5EPD_Canvas canvas(&M5.EPD);
    canvas.createCanvas(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Get time, date and sensor data: takes 15ms
    M5.RTC.getTime(&current_time);

    rtc_date_t current_date;
    M5.RTC.getDate(&current_date);
    int dow = dayofweek(current_date.year, current_date.mon, current_date.day);
    int week_number = iso_week_number(current_date.year, current_date.mon, current_date.day);

    M5.SHT30.UpdateData();

    int16_t int_hum = (int16_t)(M5.SHT30.GetRelHumidity() * 10.0);
    int16_t int_temp = (int16_t)(M5.SHT30.GetTemperature() * 10.0);
    last_temp = int_temp;

    if (low_temp > int_temp) low_temp = int_temp;
    if (high_temp < int_temp) high_temp = int_temp;

    float voltage = (float)M5.getBatteryVoltage() / 1000.0;

    // Battery-related
    // if (battery < 3200) {
    //     forced_shutdown = 1;
    //     save_persistent_data();
    //     M5.shutdown();
    // }

    // This if statement is NOT required when using battery
    if ((last_hum != int_hum) || (last_voltage != voltage) || (last_hr != current_time.hour) || (last_min != current_time.min) || (last_day != current_date.day)) {
        // Draw strings on canvas: takes 87ms
        sprintf(voltageString, "%1.2fv", voltage);
        canvas.setFreeFont(&FreeMonoBold24pt7bMini);
        canvas.drawString(voltageString, POS_VOLTAGE_X, POS_VOLTAGE_Y);

        sprintf(sensorString, "%5.1f(%3.1f|%3.1f) %5.1f%%", (float)int_temp / 10.0, (float)low_temp / 10.0, (float)high_temp / 10.0, (float)int_hum / 10.0);
        canvas.drawString(sensorString, POS_TEMP_CURRENT_X, POS_TEMP_CURRENT_Y);

        // NVR
        // if ((abs(int_temp - last_temp) >= TEMP_CHANGE_THRESHOLD || cleared || ((current_time.hour == 0) && (current_time.min == 0)))) nvr_save = 1;

        sprintf(timeString, "%02d:%02d", current_time.hour, current_time.min);
        canvas.setFreeFont(&NanumSquareRoundEB172pt7b);
        canvas.drawString(timeString, POS_TIME_X, POS_TIME_Y);

        sprintf(dateString, "%2d%02d%02d %s W%02d", current_date.year-2000, current_date.mon, current_date.day, days[dow], week_number);
        canvas.setFreeFont(&Hack_Bold58pt7bMini);
        canvas.drawString(dateString, POS_DATE_X, POS_DATE_Y);
        
        // Draw canvas on screen: takes 653ms
        if (current_time.min == 0 && !refreshed) {
            canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
            refreshed = true;
        } else {
            canvas.pushCanvas(0, 0, UPDATE_MODE_DU);
            refreshed = false;
        }

        // Sync at 6:00 AM
        if (cleared || (current_time.hour == 6 && current_time.min == 0)) sync();
    }

    
    // If using internal battery
    // if (nvr_save) save_persistent_data();
    // M5.RTC.getTime(&current_time);
    // int sleep_period = 60 - current_time.sec;
    // M5.shutdown(sleep_period);

    // In case we're plugged in and loop is still running
    last_hum = int_hum;
    last_voltage = voltage;
    last_hr = current_time.hour;
    last_min = current_time.min;
    last_day = current_date.day;
    delay(500);
}