# Headcont-with-ESP32


## Project Descripotion
The main aim of the code in this repo is to count the number of people(beacons(staff tags)) found inside a prespecified area using ESP32 BLE scanner. It was done as part of [Quasar](https://sites.google.com/view/bordaquasar/home?authuser=0) project during Borda academy 2022 internship program.

## Code Structure
The code starts by preforming a BLE scanning for three seconds. During the scannig duration, iBeacons that has the predefined UUID (so called *Staff tags*) get saved in a buffer along with their corresponding RSSI values. Upon the end of the scanning, an Interrupter gets triggered where a semaphore variable is given to the mqtt task. The mqtt task in turn, extracts the data from the buffer, turns it to the message format requested by the data team, sends it to an external server (we used raspberry pi 3 as an external server)using MQTT protocol, and finally restart the scanning for the next round.

 After receiving the desired data, the process followed by the data team was as following:
 1. Filter the received RSSI values and come with one filtered value for each staff tag.
 2. Determine an RSSI threshold limits for the prespecified area. This step is done by preforming field tests.
 3. By comparing each tag's filtered RSSI value with the threshold range, we can decide the number of the tags that are inside the desired area.
 4. Send the headcount number to the Backend team..

## Possible Future Enhancements
There are several simple changes that can be made to the code in order to make it more effective. A list of some of them follows:

1. Using Task Notification instead of Binary Semaphores. According to [FreeRTOS Documantation](https://www.freertos.org/RTOS-task-notifications.html) This can result in 45% faster performance with less RAM usage.
2. Adding Ping Pong buffer functionality. Rather than having to stop the scanning process periodically to handle the data saved to the buffer and send it via MQTT, we can maintain two buffers and switch between them as necessary. As a result, we do not need to stop scanning. Once one buffer is full or our scan duration has expired, we can simply pass the current buffer to the MQTT task and begin filling the other one.

## How To Build And Run
The SDK used in this project is [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/). Other than ESP32, no extra hardware is required. To install the toolchain with vs code, you can follow this [guide](https://www.youtube.com/watch?v=Lc6ausiKvQM) published by Espressif Systems. For scanning purpose, you need any device that advertise an iBeacon packet. Don't forget to make sure that your beacons have the same UUID as the one assigned to STAFF_TAG_COMMON_UUID macro in the code, otherwise they will be ignored by the scanner. HAVE FUN.
 

