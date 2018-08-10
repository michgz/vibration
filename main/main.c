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

const static char *TAG = "TASK_MAIN";


static bool IsDecimalFileName(char * pStr)
{
    return (strlen(pStr) == 4);
}


void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );

    ESP_LOGI(TAG, "Initializing SPIFFS");
    
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Use POSIX and C standard library functions to work with files.

    // Check if destination file exists before renaming
    //struct stat st;
    //if (stat("/spiffs/foo.txt", &st) == 0) {
    //    // Delete it if it exists
    //    unlink("/spiffs/foo.txt");
    //}


    /* Print a list of the directory    */

    /* spiffs_DIR */ void *pDir;
    struct dirent *pDirent;

    pDir = opendir ("/spiffs");
    if (pDir == NULL) {
        ESP_LOGI(TAG, "Cannot open directory '%s'\n", "/spiffs");
        return;
    }

    int max_i = 0;

    while ((pDirent = readdir(pDir)) != NULL) {
        ESP_LOGI(TAG, "[%s]", pDirent->d_name);
        int i = 0;
        if (IsDecimalFileName(pDirent->d_name))
        {
            sscanf(pDirent->d_name, "%d", &i);
        }
        if (i > max_i)
            max_i = i;
    }
    closedir (pDir);

    // Create a file.
    ESP_LOGI(TAG, "Opening file");
    {
    char c[24];
    sprintf(c, "/spiffs/%04d", max_i + 1);
    FILE* f = fopen(c, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "1,0,0,256,0\n");
    fclose(f);
    }
    ESP_LOGI(TAG, "File written");

    // Open renamed file for reading
    //ESP_LOGI(TAG, "Reading file");
    //f = fopen("/spiffs/foo.txt", "r");
    //if (f == NULL) {
    //    ESP_LOGE(TAG, "Failed to open file for reading");
    //    return;
    //}
    //char line[64];
    //fgets(line, sizeof(line), f);
    //fclose(f);
    // strip newline
    //char* pos = strchr(line, '\n');
    //if (pos) {
    //    *pos = '\0';
    //}
    //ESP_LOGI(TAG, "Read from file: '%s'", line);

    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "SPIFFS unmounted");

    wifi_conn_init();
}
