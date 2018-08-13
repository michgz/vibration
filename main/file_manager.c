#include "file_manager.h"

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

const static char *TAG = "FILE_MANAGER";

void fm_delete_all_files(void)
{
    esp_spiffs_format(NULL);  // format the default partition
}
