#include "main.h"


#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "openssl/ssl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"

#include "esp_attr.h"
#include "soc/rtc.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs_flash.h"

#include "task_wifi.h"
#include "file_manager.h"



void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    
    // Create the file system if it is not already created, and if it is then find
    // the largest file number that exists.
    fm_init();

    wifi_conn_init();
}
