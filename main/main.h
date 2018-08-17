#ifndef _MAIN_H_
#define _MAIN_H_

#include "sdkconfig.h"
#include <time.h>

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID                CONFIG_ESP_WIFI_SSID
#define EXAMPLE_WIFI_PASS                CONFIG_ESP_WIFI_PASSWORD

#define MAIN_LOOP_TASK_NAME        "main_loop"
#define MAIN_LOOP_TASK_STACK_WORDS 10240
#define MAIN_LOOP_TASK_PRIORITY    8

#define MAIN_LOOP_RECV_BUF_LEN       1024

#define MAIN_LOOP_LOCAL_HTTP_PORT     80

typedef struct {

    float freq_requested;   // Units Hz
    float ampl_requested;   // Units dB
    float freq_used;        // Units Hz
    float ampl_used;        // Units dB
    
    int   result;    // 0 = fail, 1 = success


} OPERATION_t ;

typedef struct {
    float t;    // Units seconds
    float x;    // Units g
    float y;    // Units g
    float z;    // Units g

} SINGLE_RESULT_t ;

typedef struct {

    float freq_used;        // Units Hz
    float ampl_used;        // Units dB
    time_t   time;
    SINGLE_RESULT_t res [500];

} RESULTS_FILE_t ;

typedef struct {
    int index;
    float x, y, z;
    
} READING_t ;

typedef struct {
    float freq;
    float ampl;
    time_t  time;
    
    READING_t read [500];

} FILE_STRUCT_t;


#endif  //_MAIN_H_
