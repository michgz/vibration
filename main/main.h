#ifndef _MAIN_H_
#define _MAIN_H_

#include "sdkconfig.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID                CONFIG_ESP_WIFI_SSID
#define EXAMPLE_WIFI_PASS                CONFIG_ESP_WIFI_PASSWORD

#define OPENSSL_EXAMPLE_TASK_NAME        "openssl_example"
#define OPENSSL_EXAMPLE_TASK_STACK_WORDS 10240
#define OPENSSL_EXAMPLE_TASK_PRIORITY    8

#define OPENSSL_EXAMPLE_RECV_BUF_LEN       1024

#define OPENSSL_EXAMPLE_LOCAL_TCP_PORT     443

typedef struct {

    float freq_requested;   // Units Hz
    float ampl_requested;   // Units dB
    float freq_used;        // Units Hz
    float ampl_used;        // Units dB
    
    int   result;    // 0 = fail, 1 = success


} OPERATION_t ;


#endif  //_MAIN_H_
