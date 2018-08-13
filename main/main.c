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
#include "task_pwm.h"
#include "task_sensor.h"
#include "file_manager.h"

const static char *TAG = "TASK_MAIN";



void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    
    // Create the file system if it is not already created, and if it is then find
    // the largest file number that exists.
    fm_init();

//    app_main_task_sensor();

    wifi_conn_init();
}
