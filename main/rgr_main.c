#include <stdio.h>
#include "secrets.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/ledc.h"         // Драйвер ШІМ (PWM)
#include "nvs_flash.h"           // Пам'ять для Wi-Fi
#include "esp_wifi.h"            // Wi-Fi
#include "mqtt_client.h"         // MQTT
#include "cJSON.h"               // Формування JSON
#include "esp_sntp.h"            // Синхронізація часу
#include "esp_crt_bundle.h"      // ДОДАНО: Вбудоване сховище сертифікатів SSL/TLS
#include "esp_log.h"

// === НАЛАШТУВАННЯ МЕРЕЖІ ===
#define WIFI_SSID                   SECRET_WIFI_SSID      
#define WIFI_PASS                   SECRET_WIFI_PASS  


// === НАЛАШТУВАННЯ AZURE IOT HUB ===
#define AZURE_HOST      SECRET_AZURE_HOST
#define AZURE_DEVICE_ID SECRET_AZURE_DEVICE_ID
#define AZURE_USERNAME  AZURE_HOST "/" AZURE_DEVICE_ID "/?api-version=2021-04-12"
#define AZURE_PASSWORD  SECRET_AZURE_PASSWORD 
#define AZURE_TOPIC     "devices/" AZURE_DEVICE_ID "/messages/events/"

// === НАЛАШТУВАННЯ ПЕРИФЕРІЇ ===
#define I2C_MASTER_SDA_IO           1      
#define I2C_MASTER_SCL_IO           2      
#define I2C_MASTER_FREQ_HZ          100000 
#define SCD41_ADDR                  0x62
#define OLED_ADDR                   0x3C
#define MQ135_ADC_CHANNEL           ADC_CHANNEL_3 // GPIO 4

// Налаштування кулера (GPIO 5)
#define FAN_PWM_PIN                 5
#define FAN_LEDC_TIMER              LEDC_TIMER_0
#define FAN_LEDC_CHANNEL            LEDC_CHANNEL_0

static const char *TAG = "AIR_SYSTEM";

// Хендли
i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t scd41_handle;
i2c_master_dev_handle_t oled_handle;
adc_oneshot_unit_handle_t adc1_handle;
esp_mqtt_client_handle_t mqtt_client;

// === 1. БЛОК OLED ===
static void oled_send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    i2c_master_transmit(oled_handle, buf, 2, -1);
}

static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5f,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7f,0x14,0x7f,0x14},
    {0x24,0x2a,0x7f,0x2a,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1c,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1c,0x00}, {0x14,0x08,0x3e,0x08,0x14}, {0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3e}, {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36}, {0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c}, {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01}, {0x3e,0x41,0x49,0x49,0x7a},
    {0x7f,0x08,0x08,0x08,0x7f}, {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01}, {0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40}, {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f}, {0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06}, {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01}, {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f}, {0x3f,0x40,0x38,0x40,0x3f},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, 
    {0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},
    {0x20,0x54,0x54,0x54,0x78}, {0x7f,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7f},
    {0x38,0x54,0x54,0x54,0x18}, {0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},
    {0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},
    {0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0}
};

static void oled_clear() {
    uint8_t blank[129]; blank[0] = 0x40; memset(&blank[1], 0x00, 128);
    for (uint8_t page = 0; page < 4; page++) {
        oled_send_cmd(0xB0 + page); oled_send_cmd(0x00); oled_send_cmd(0x10);
        i2c_master_transmit(oled_handle, blank, 129, -1);
    }
}
static void oled_set_cursor(uint8_t page, uint8_t col) {
    oled_send_cmd(0xB0 | (page & 0x07)); oled_send_cmd(0x00 | (col & 0x0F)); oled_send_cmd(0x10 | ((col >> 4) & 0x0F));
}
static void oled_print_char(char c) {
    if (c < 32 || c > 122) c = 32; 
    uint8_t data[6]; data[0] = 0x40; 
    for (int i = 0; i < 5; i++) data[i+1] = font5x7[c - 32][i];
    i2c_master_transmit(oled_handle, data, 6, -1);
    uint8_t space[2] = {0x40, 0x00}; i2c_master_transmit(oled_handle, space, 2, -1);
}
static void oled_print_str(uint8_t page, uint8_t col, const char* str) {
    oled_set_cursor(page, col);
    while (*str) oled_print_char(*str++);
}

// === 2. БЛОК ШІМ (PWM) ДЛЯ КУЛЕРА ===
static void fan_pwm_init() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = FAN_LEDC_TIMER,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 25000,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = FAN_LEDC_CHANNEL,
        .timer_sel      = FAN_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = FAN_PWM_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGI(TAG, "ШІМ вентилятора ініціалізовано на GPIO %d", FAN_PWM_PIN);
}

static void set_fan_speed(uint8_t duty) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, FAN_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, FAN_LEDC_CHANNEL);
}

// === 3. БЛОК WI-FI ТА MQTT ===
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "Втрачено з'єднання. Перепідключення до Wi-Fi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi підключено успішно!");
    }
}

static void wifi_init() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mqtt_init() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://" AZURE_HOST ":8883", 
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach, // Зчитуємо сертифікати з мікроконтролера
        .credentials = {
            .client_id = AZURE_DEVICE_ID,
            .username = AZURE_USERNAME,
            .authentication.password = AZURE_PASSWORD,
        }
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT клієнт Azure запущено!");
}

// === 4. ІНІЦІАЛІЗАЦІЯ ПЕРИФЕРІЇ ===
static void periph_init() {
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT, .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_MASTER_SCL_IO, .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    i2c_device_config_t scd_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = SCD41_ADDR, .scl_speed_hz = I2C_MASTER_FREQ_HZ };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &scd_cfg, &scd41_handle));

    i2c_device_config_t oled_cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = OLED_ADDR, .scl_speed_hz = I2C_MASTER_FREQ_HZ };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &oled_cfg, &oled_handle));

    adc_oneshot_unit_init_cfg_t adc_init_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_cfg, &adc1_handle));
    adc_oneshot_chan_cfg_t adc_ch_cfg = { .bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MQ135_ADC_CHANNEL, &adc_ch_cfg));
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    
    // Ініціалізація підсистем
    periph_init();
    fan_pwm_init();
    wifi_init();

    // --- СИНХРОНІЗАЦІЯ ЧАСУ ДЛЯ AZURE (NTP) ---
    ESP_LOGI(TAG, "Синхронізація часу через NTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Запускаємо цикл очікування, поки час не синхронізується
    int retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 15) {
        ESP_LOGI(TAG, "Очікування NTP... (%d/15)", retry);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    // ------------------------------------------

    mqtt_init();

    // Запуск дисплея
    oled_send_cmd(0xAE); oled_send_cmd(0xD5); oled_send_cmd(0x80); oled_send_cmd(0xA8); oled_send_cmd(0x1F); 
    oled_send_cmd(0xD3); oled_send_cmd(0x00); oled_send_cmd(0x40); oled_send_cmd(0x8D); oled_send_cmd(0x14); 
    oled_send_cmd(0x20); oled_send_cmd(0x02); oled_send_cmd(0xA1); oled_send_cmd(0xC8); oled_send_cmd(0xDA); 
    oled_send_cmd(0x02); oled_send_cmd(0x81); oled_send_cmd(0x8F); oled_send_cmd(0xAF); 
    oled_clear();

    // Запуск SCD41
    // === ЗАПУСК SCD41 З ДІАГНОСТИКОЮ ===
    ESP_LOGI(TAG, "Ініціалізація SCD41...");
    uint8_t cmd_stop[2] = {0x3f, 0x86}; 
    esp_err_t err = i2c_master_transmit(scd41_handle, cmd_stop, 2, pdMS_TO_TICKS(1000));
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SCD41 НЕ ВІДПОВІДАЄ! Перевір контакти. Помилка: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "SCD41 знайдено на шині I2C.");
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    uint8_t cmd_start[2] = {0x21, 0xb1}; 
    i2c_master_transmit(scd41_handle, cmd_start, 2, pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Очікування 5 секунд для першого виміру SCD41...");
    vTaskDelay(pdMS_TO_TICKS(5000)); // КРИТИЧНО ВАЖЛИВА ЗАТРИМКА

    while (1) {
        // Збір даних MQ135
        int voc_raw = 0;
        adc_oneshot_read(adc1_handle, MQ135_ADC_CHANNEL, &voc_raw);

        // Збір даних SCD41
        // --- 1. ПЕРЕВІРКА ГОТОВНОСТІ ДАНИХ (Data Ready Status) ---
        uint8_t cmd_ready[2] = {0xe4, 0xb8};
        uint8_t ready_data[3];
        bool data_ready = false;

        if (i2c_master_transmit(scd41_handle, cmd_ready, 2, pdMS_TO_TICKS(100)) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(2));
            if (i2c_master_receive(scd41_handle, ready_data, 3, pdMS_TO_TICKS(100)) == ESP_OK) {
                uint16_t status = (ready_data[0] << 8) | ready_data[1];
                if ((status & 0x07FF) != 0) { // Якщо не нуль – дані готові
                    data_ready = true;
                }
            }
        }

        // --- 2. ЧИТАННЯ ДАНИХ ТІЛЬКИ ЯКЩО ВОНИ ГОТОВІ ---
        uint16_t co2 = 0; float temp = 0, hum = 0;

        if (data_ready) {
            uint8_t cmd_read[2] = {0xec, 0x05};
            esp_err_t tx_err = i2c_master_transmit(scd41_handle, cmd_read, 2, pdMS_TO_TICKS(100));
            
            if (tx_err == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(2)); // Даємо датчику час підготувати байти
                uint8_t data[9];
                esp_err_t rx_err = i2c_master_receive(scd41_handle, data, 9, pdMS_TO_TICKS(100));
                
                if (rx_err == ESP_OK) {
                    co2 = (data[0] << 8) | data[1];
                    temp = -45.0 + 175.0 * (float)((data[3] << 8) | data[4]) / 65535.0;
                    hum = 100.0 * (float)((data[6] << 8) | data[7]) / 65535.0;
                    // ESP_LOGI(TAG, "SCD41 OK: CO2=%d", co2); // За бажанням можна розкоментувати
                } else {
                    ESP_LOGW(TAG, "Помилка i2c_master_receive: %s", esp_err_to_name(rx_err));
                }
            } else {
                ESP_LOGW(TAG, "Датчик відхилив read_measurement: %s", esp_err_to_name(tx_err));
            }
        } else {
            ESP_LOGW(TAG, "SCD41: Дані ще не готові");
        }

        // --- ЛОГІКА КЕРУВАННЯ ВЕНТИЛЯТОРОМ ---
        uint8_t fan_speed = 0;
        if (co2 > 1200 || voc_raw > 2000) {
            fan_speed = 255; 
        } else if (co2 > 800 || voc_raw > 1000) {
            fan_speed = 127; 
        } else {
            fan_speed = 0;   
        }
        set_fan_speed(fan_speed);

        // --- ФОРМУВАННЯ JSON ТА ВІДПРАВКА ПО MQTT ---
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "co2", co2);
        cJSON_AddNumberToObject(root, "temperature", temp);
        cJSON_AddNumberToObject(root, "humidity", hum);
        cJSON_AddNumberToObject(root, "voc", voc_raw);
        cJSON_AddNumberToObject(root, "fan_pwm", fan_speed);

        char *json_string = cJSON_PrintUnformatted(root);
        esp_mqtt_client_publish(mqtt_client, AZURE_TOPIC, json_string, 0, 1, 0); 
        
        ESP_LOGI(TAG, "Відправлено в Azure: %s", json_string);

        cJSON_Delete(root);
        free(json_string);

        // --- ОНОВЛЕННЯ ДИСПЛЕЯ ---
        char line1[32], line2[32], line3[32], line4[32];
        snprintf(line1, sizeof(line1), "CO2: %d PPM", co2);
        snprintf(line2, sizeof(line2), "TEMP: %.1f C", temp);
        snprintf(line3, sizeof(line3), "HUM: %.1f %%", hum);
        
        // Об'єднуємо VOC та швидкість вентилятора на 4-му рядку
        snprintf(line4, sizeof(line4), "VOC:%d FAN:%d%%", voc_raw, (fan_speed * 100) / 255);

        oled_print_str(0, 0, "                    "); oled_print_str(0, 0, line1);
        oled_print_str(1, 0, "                    "); oled_print_str(1, 0, line2);
        oled_print_str(2, 0, "                    "); oled_print_str(2, 0, line3);
        oled_print_str(3, 0, "                    "); oled_print_str(3, 0, line4);
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}