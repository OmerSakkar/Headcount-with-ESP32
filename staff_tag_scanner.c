#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "protocol_examples_common.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_ibeacon_api.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/timer.h"
#include "mqtt_client.h"
#include "staff_tag.h"
#include "wifi_station.h"

#define TIMER_DIVIDER   (16)
#define TIMER_SCALE     (TIMER_BASE_CLK / TIMER_DIVIDER) 
#define SAFE_DURATION   ((20)*(TIMER_SCALE)/1000)  
#define STAFF_TAG_TOPIC "/o1/st" 

#define MQTT_BROKER_URI ""
#define MQTT_BROKER_USER_NAME ""
#define MQTT_BROKER_PASSWORD ""


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

static const char* DEMO_TAG = "IBEACON_DEMO";

static SemaphoreHandle_t timer_sem;
static volatile uint16_t num_of_tags = 0; //the number of unique tags encountered so far in one scanning duration 
staff_tag_t scanned_tags[MAX_TAG_NUMBERS];
esp_mqtt_client_handle_t staff_tag_client;

const esp_mqtt_client_config_t mqtt_config = {
        .uri = MQTT_BROKER_URI ,
        .username = MQTT_BROKER_USER_NAME ,
        .password = MQTT_BROKER_PASSWORD
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,     
    .scan_window            = 0x30,
    .scan_duplicate         =  BLE_SCAN_DUPLICATE_DISABLE 
};

void ble_ibeacon_appRegister(void)
{
    esp_err_t status;
    ESP_LOGI(DEMO_TAG, "register callback");
    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(DEMO_TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }
}
void ble_ibeacon_init(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_ibeacon_appRegister();
}


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        uint32_t duration = SCAN_DURATION_IN_S; 
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Scan start failed: %s", esp_err_to_name(err));
        }
        else{
            ESP_LOGI(DEMO_TAG, "**Scan STARTED**");
            timer_start(TIMER_GROUP_0, TIMER_0); 
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            
            if ( ( esp_ble_is_staff_tag_packet(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len) )  ){
                esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(scan_result->scan_rst.ble_adv);
                ESP_LOGI(DEMO_TAG, "----------staff tag found----------");
        
                uint16_t major = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.major);
                uint16_t minor = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.minor);
                int8_t measured_power = ibeacon_data->ibeacon_vendor.measured_power;
                int rssi = scan_result->scan_rst.rssi;

                uint16_t tag_index; //tracks the index of the current tag
                tag_index = num_of_tags;
                ESP_LOGI(DEMO_TAG, "tag_index = %d",tag_index);
                for(uint16_t i = 0; i<num_of_tags; i++){
                    ESP_LOGI(DEMO_TAG, "at least we're here");
                    if(scanned_tags[i].minor == minor && scanned_tags[i].major == major ){ //if the tag was found before during this scan duration
                        tag_index = i; 
                    }
                }
                if(tag_index == num_of_tags){ //if the current tag has never been encountered in this scan duration
                    ESP_LOGI(DEMO_TAG, "First time encuontering this tag");        
                    if(num_of_tags == MAX_TAG_NUMBERS){
                        ESP_LOGE(DEMO_TAG,"Tag's Buffer is already full.");
                        break;
                    }
                    else{
                        scanned_tags[tag_index].size = 0; 
                        scanned_tags[tag_index].major = major; 
                        scanned_tags[tag_index].minor = minor;
                        scanned_tags[tag_index].measured_power = measured_power;
                        num_of_tags++;
                        ESP_LOGI(DEMO_TAG, "Incerementing num_of_tags");
                    }
                }
                if(scanned_tags[tag_index].size == TOTAL_SAMPLE_NUMBER){
                    ESP_LOGE(DEMO_TAG, "RSSI buffer is full for %u. tag",tag_index);
                    break;
                }
                scanned_tags[tag_index].rssi[scanned_tags[tag_index].size] = rssi;
                scanned_tags[tag_index].size += 1;
                ESP_LOGI(DEMO_TAG, "Major: 0x%04x (%d)", major, major);
                ESP_LOGI(DEMO_TAG, "Minor: 0x%04x (%d)", minor, minor);
                ESP_LOGI(DEMO_TAG, "Measured power (RSSI at a 1m distance):%d dbm", ibeacon_data->ibeacon_vendor.measured_power);
                ESP_LOGI(DEMO_TAG, "RSSI of packet:%d dbm", scan_result->scan_rst.rssi);
                ESP_LOGI(DEMO_TAG, "INSIDE num_of_tags = %u", num_of_tags);
            }
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

/* gets triggered at the end of each scan duration. It then pauses 
   the timer and give the semaphore to the data handling task */
static void IRAM_ATTR timer_group_isr_callback(void *args){
    timer_pause(TIMER_GROUP_0, TIMER_0);

    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(timer_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/* this task goes through scanned tags one by one saves each tag's major, minor 
   and rssi values to the buffer and sends them to the mqtt client. Finally it 
   resets the scan parameters and restart the scanning.                            */
static void mqtt_publish_task(void *arg)
{
    while(1)
    {
        //TODO use Task Notifications instead of binary semaphore
        if( xSemaphoreTake(timer_sem, portMAX_DELAY) )
        {
            if (num_of_tags == 0){
                esp_mqtt_client_publish(staff_tag_client, STAFF_TAG_TOPIC ,"0", 0, 1, 0);
            }
            else
            {
                /*message format: major1,minor1,rssi11,rssi2,.....;major2,minor2,rssi1,rssi2.......*/
                char message_buffer[MQTT_TOTAL_MESSAGE_LENGHT]= ""; 
                uint16_t length = 0 ;
                ESP_LOGI(DEMO_TAG, "num_of_tags = %u", num_of_tags);
                for(uint16_t i=0; i<num_of_tags; i++ ){
                    length += snprintf(message_buffer+length, MQTT_TOTAL_MESSAGE_LENGHT-length, ";%d,%d,%d", scanned_tags[i].major, , scanned_tags[i].minor, scanned_tags[i].measured_power);
                    for(uint16_t j=0; j<scanned_tags[i].size; j++ ){
                        length += snprintf(message_buffer+length,  MQTT_TOTAL_MESSAGE_LENGHT-length, ",%d", scanned_tags[i].rssi[j]); //rssi1,rssi2,....
                    } 
                }
                esp_mqtt_client_publish(staff_tag_client, STAFF_TAG_TOPIC , message_buffer, 0, 1, 0);              
                reset_staff_tags(scanned_tags, num_of_tags); //*important for size validity checking in esp callback function
                num_of_tags = 0;
            }
        esp_ble_gap_start_scanning(SCAN_DURATION_IN_S);        
        }
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("IBEACON_DEMO", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    timer_sem = xSemaphoreCreateBinary();
    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(wifi_init_sta());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0); 

    
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0,(SCAN_DURATION_IN_S*TIMER_SCALE)+SAFE_DURATION); 
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_group_isr_callback, NULL, 0);


    staff_tag_client = esp_mqtt_client_init(&mqtt_config);
    ESP_ERROR_CHECK(esp_mqtt_client_start(staff_tag_client));
    ble_ibeacon_init();

    esp_ble_gap_set_scan_params(&ble_scan_params);

    xTaskCreatePinnedToCore(mqtt_publish_task, "mqtt_task", 4096, NULL, 1, NULL, 1);
}

                