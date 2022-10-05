#ifndef STAFF_TAG_H
#define STAFF_TAG_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_log.h"

#define MY_SCAN_INTERVAL          (50)
#define MY_SCAN_WINDOW            (30)
#define TOTAL_SAMPLE_NUMBER       (10) 
#define SCAN_DURATION_IN_MS       3000 
#define SCAN_DURATION_IN_S        (SCAN_DURATION_IN_MS/1000)  //duration can't be a fraction of a second. 
#define MAX_TAG_NUMBERS           (25)  
#define MQTT_TAG_MESSAGE_LENGTH   (75)  //(40byte RSSI value) + 4byte (Tx) + 20 (virgul) + 10(major+minor) = 60 byte //message lenght for 1 tag
#define MQTT_TOTAL_MESSAGE_LENGHT (MAX_TAG_NUMBERS*MQTT_TAG_MESSAGE_LENGTH)
#define STAFF_TAG_COMMON_UUID     {0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a} //ch 

typedef struct {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t company_id;
    uint16_t beacon_type;
    uint8_t proximity_uuid[16]; 
}__attribute__((packed)) esp_ble_staff_tag_t; 

typedef struct {
    int rssi[TOTAL_SAMPLE_NUMBER]; 
    size_t size; //size of rrsi array
    uint16_t major;
    uint16_t minor;
    int8_t measured_power;
} staff_tag_t;


void reset_staff_tags(staff_tag_t* staff_tags_arr, uint16_t number_of_items);
bool esp_ble_is_staff_tag_packet (uint8_t *adv_data, uint8_t adv_data_len);



#endif


