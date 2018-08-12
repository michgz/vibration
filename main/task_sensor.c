#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

#include "sdkconfig.h"

#include "esp_spiffs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#include "driver/i2c.h"
#include "task_sensor.h"

/**
 *
 * Pin assignment:
 *
 * - master:
 *    GPIO18 is assigned as the data signal of i2c master port
 *    GPIO19 is assigned as the clock signal of i2c master port
 *
 */

#define I2C_MASTER_SCL_IO          19               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          18               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM             I2C_NUM_1        /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE  0                /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE  0                /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ         1000000          /*!< I2C master clock frequency */

#define ADXL355_SENSOR_ADDR   0x1D
#define ADXL355_CMD_DEVID_AD  0x00
#define ADXL355_CMD_FILTER    0x28
#define ADXL355_CMD_POWER_CTL 0x2D
#define ADXL355_CMD_XDATA3    0x08

#define ADXL355_CMD_STATUS 0x04


#define WRITE_BIT                          I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                           I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                       0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                      0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                            0x0              /*!< I2C ack value */
#define NACK_VAL                           0x1              /*!< I2C nack value */

const static char *TAG = "TASK_SENSOR";

static void example_tg1_timer_deinit(int timer_idx);


/* Write a value into a register.   */
static esp_err_t i2c_adxl_write_single_register(i2c_port_t i2c_num, uint8_t addr, uint8_t data_wr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ADXL355_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data_wr, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* Read a value from a register.   */
static esp_err_t i2c_adxl_read_single_register(i2c_port_t i2c_num, uint8_t addr, uint8_t *data_rd)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ADXL355_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    //vTaskDelay(30 / portTICK_RATE_MS);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ADXL355_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data_rd[0], NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* Read several values from consecutive registers.   */
static esp_err_t i2c_adxl_read_multiple(i2c_port_t i2c_num, uint8_t addr, uint8_t *data_rd, uint8_t size)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ADXL355_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    //vTaskDelay(30 / portTICK_RATE_MS);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ADXL355_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read(cmd, data_rd, size, ACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}


/* Read addresses 0x00 - 0x03. On the ADXL355, they should read AD 1D ED 01.   */
static esp_err_t i2c_adxl_read_device_id(i2c_port_t i2c_num, uint8_t data[4])
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ADXL355_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, ADXL355_CMD_DEVID_AD, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    //vTaskDelay(30 / portTICK_RATE_MS);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ADXL355_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data[0], ACK_VAL);
    i2c_master_read_byte(cmd, &data[1], ACK_VAL);
    i2c_master_read_byte(cmd, &data[2], ACK_VAL);
    i2c_master_read_byte(cmd, &data[3], NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}


/**
 * @brief i2c master initialization
 */
static void i2c_example_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);
}


/* Decode the 3-byte reading into a value in g. Relies on the range being
 * as set by default (register 0x2C), which is +-2g.    */
static float adxl_decode_reading(uint8_t raw_data[3])
{
    uint32_t z;

    z = (((uint32_t)raw_data[0]) << 12)   +
         (((uint32_t)raw_data[1]) << 4)   +
         (((uint32_t)raw_data[2]) >> 4);

    if ((z & 0x80000UL) == 0)
    {
        // The reading is >= 0
        return  ((float) (z & 0x7FFFFUL)) / 256000.0;
    }
    else
    {
        // The reading is < 0
        z = -z;
        return  -1.0 * (((float) (z & 0x7FFFFUL)) / 256000.0);
    }

}


typedef struct {
    int index;
    float x, y, z;
    
} READING_t ;

typedef struct {
    float freq;
    float ampl;
    
    READING_t read [500];

} FILE_STRUCT_t;



static FILE_STRUCT_t   theFile;



static int write_into_a_file(int max_i, FILE_STRUCT_t * f_in)
{

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
    fwrite(f_in, sizeof(FILE_STRUCT_t)/sizeof(float), sizeof(float), f);
    fclose(f);
    }
    ESP_LOGI(TAG, "File written");


    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "SPIFFS unmounted");

}

static void i2c_test_task(void* arg)
{
    int ret;
    uint8_t dev_id[4];

    int count_readings = 0;
    int count_notifications = 0;

    ret = i2c_adxl_read_device_id(I2C_MASTER_NUM, dev_id);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Detected sensor: %02X %02X %02X %02X", dev_id[0],
                                                                dev_id[1],
                                                                dev_id[2],
                                                                dev_id[3]);

        if (0 == memcmp(dev_id, "\xad\x1d\xed\x01", 4))
        {
            ESP_LOGI(TAG, "Recognised as a ADXL355 accelerometer");

            (void) i2c_adxl_write_single_register(I2C_MASTER_NUM, ADXL355_CMD_FILTER, 0x05);  // Set up NO high-pass filter, 125Hz data rate

            (void) i2c_adxl_write_single_register(I2C_MASTER_NUM, ADXL355_CMD_POWER_CTL, 2);  // Start measuring

            theFile.freq = 1.0;
            theFile.ampl = 0.0;

            while(count_readings<500)
            {

                (void) ulTaskNotifyTake( pdFALSE, portMAX_DELAY );

                //ESP_LOGI(TAG, "Notification Given");

                count_notifications ++;

                uint8_t status_byte;

                if (ESP_OK == i2c_adxl_read_single_register(I2C_MASTER_NUM, ADXL355_CMD_STATUS, &status_byte))
                {
                    //ESP_LOGI(TAG, "before status = %02X", status_byte);

                    if ((status_byte & 0x01) != 0)
                    {
                        uint8_t readings [9];

                        (void) i2c_adxl_read_multiple(I2C_MASTER_NUM, ADXL355_CMD_XDATA3, readings, 9);

                        //ESP_LOGI(TAG, "Reading: %0.6f, %0.6f, %0.6f", adxl_decode_reading(&readings[0]), adxl_decode_reading(&readings[3]), adxl_decode_reading(&readings[6]));
                        
                        theFile.read[count_readings].index = count_readings;
                        theFile.read[count_readings].x = adxl_decode_reading(&readings[0]);
                        theFile.read[count_readings].y = adxl_decode_reading(&readings[3]);
                        theFile.read[count_readings].z = adxl_decode_reading(&readings[6]);
                                                
                        count_readings ++;

                        //(void) i2c_adxl_read_single_register(I2C_MASTER_NUM, ADXL355_CMD_STATUS, &status_byte);
                        //ESP_LOGI(TAG, "after status = %02X", status_byte);
                    }
                }
            }

            ESP_LOGI(TAG, "Got %d readings with %d notifications", count_readings, count_notifications);

            (void) i2c_adxl_write_single_register(I2C_MASTER_NUM, ADXL355_CMD_POWER_CTL, 3);  // Stop measuring

            // Now write into the file
            (void) write_into_a_file(1, &theFile);

        }
        else
        {
            ESP_LOGI(TAG, "Not recognised as any known accelerometer");
        }
    }

    example_tg1_timer_deinit(TIMER_0);

    vTaskDelete(NULL);

}


#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.001)   // sample test interval for the first timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload
#define TEST_CONTINUOUS       2



TaskHandle_t task_to_notify;


/*
 * Timer group1 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group1_isr(void *para)
{
    int timer_idx = (int) para;


    if (timer_idx == TIMER_0)
    {
        TIMERG1.int_clr_timers.t0 = 1;
    }
    else
    {
        TIMERG1.int_clr_timers.t1 = 1;
    }


    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG1.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    portBASE_TYPE wokenHigherPriority = 0;

    /* Now just send the event data back to the main program task */
    if (timer_idx == TIMER_0)
    {
        xTaskNotifyFromISR(task_to_notify, 1, eIncrement, &wokenHigherPriority);
    }

    if (wokenHigherPriority)
    {
        portYIELD_FROM_ISR ();
    }
}


/*
 * Initialize selected timer of the timer group 1
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
static void example_tg1_timer_init(int timer_idx, 
    int auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = (auto_reload != TEST_CONTINUOUS) ? TIMER_ALARM_EN : 0;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = (auto_reload == TEST_WITH_RELOAD);
    timer_init(TIMER_GROUP_1, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_1, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    if (auto_reload != TEST_CONTINUOUS)
    {
        timer_set_alarm_value(TIMER_GROUP_1, timer_idx, timer_interval_sec * TIMER_SCALE);
        timer_enable_intr(TIMER_GROUP_1, timer_idx);
        timer_isr_register(TIMER_GROUP_1, timer_idx, timer_group1_isr, 
            (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);
    }

    timer_start(TIMER_GROUP_1, timer_idx);
}

static void example_tg1_timer_deinit(int timer_idx)
{
    timer_pause(TIMER_GROUP_1, timer_idx);
}

void app_main_task_sensor()
{
    i2c_example_master_init();


    xTaskCreate(i2c_test_task, "i2c_test_task_0", 1024 * 2, (void* ) 0, 10, &task_to_notify);

    example_tg1_timer_init(TIMER_0, TEST_WITH_RELOAD, TIMER_INTERVAL0_SEC);

}

