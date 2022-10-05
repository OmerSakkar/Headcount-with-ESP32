#include "staff_tag.h"

static const char* DEMO_TAG = "IBEACON_DEMO";

//all staff tags share the same: head(ibeacon head) + UUID
esp_ble_staff_tag_t staff_tag_common_data = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x004C,
    .beacon_type = 0x1502,
    .proximity_uuid = STAFF_TAG_COMMON_UUID
};


void reset_staff_tags(staff_tag_t* staff_tags_arr, uint16_t number_of_items)
{
    for(uint16_t i =0; i<number_of_items; i++)
        (staff_tags_arr+i)->size = 0; // = *(staff_tags_arr+i).size=0;
        
}

bool esp_ble_is_staff_tag_packet (uint8_t *adv_data, uint8_t adv_data_len){
    bool result = false;
    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
        if (!memcmp(adv_data, (uint8_t*)&staff_tag_common_data, sizeof(staff_tag_common_data))){
            result = true;
        }
    }
    return result;
}
