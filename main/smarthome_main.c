/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_spi_flash.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT";

static char mqtt_status_buf[64];
static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t mqtt_event_group;

const int CONNECTED_BIT = BIT0;
const int MQTT_STATUS_BIT = BIT1;

static int mqtt_online = 0;

static esp_mqtt_client_handle_t client;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    system_event_info_t *info = &event->event_info;
    switch(event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "Disconnect reason: %d", info->disconnected.reason);
            if(info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT)
            {
                esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
            }
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    memset(mqtt_status_buf, 0x00, sizeof(mqtt_status_buf));
    switch(event->event_id)
    {
        case MQTT_EVENT_BEFORE_CONNECT:
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECT");
            mqtt_online = 1;
            esp_mqtt_client_subscribe(client, "/esp8266/set", 0);
            strcpy(mqtt_status_buf, "MQTT CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECT");
            mqtt_online = 0;
            strcpy(mqtt_status_buf, "MQTT DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            strcpy(mqtt_status_buf, "MQTT SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            strcpy(mqtt_status_buf, "MQTT UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            strcpy(mqtt_status_buf, "MQTT PUBLISH");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            strcpy(mqtt_status_buf, "MQTT RECEIVE");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            strcpy(mqtt_status_buf, "MQTT ERROR");
            break;
        default:
            break;
    }
    xEventGroupSetBits(mqtt_event_group, MQTT_STATUS_BIT);
    return ESP_OK;
}

static void mqtt_app_start(void)
{
    mqtt_event_group = xEventGroupCreate();    
    esp_mqtt_client_config_t mqtt_cfg =
    {
        .host = "111.230.206.15",
        .port = 1883,
        .username = "panda",
        .password = "panda",
        .event_handle = mqtt_event_handler,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

}


static void wifi_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config =
    {
        .sta={
            .ssid = "Panda",
            .password = "18188954638"
        },
    };

    ESP_LOGI(TAG, "WIFI SSID: %s ...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}


void app_main()
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP8266 chip with %d CPU cores, WiFi silicon revision %d, %d MB %s flash\n", 
        chip_info.cores, chip_info.revision, spi_flash_get_chip_size() / (1024 * 1024)
        ,(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    wifi_init();
    mqtt_app_start();
}