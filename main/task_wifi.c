#include "task_wifi.h"
#include "main.h"


#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "lwip/api.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "task_pwm.h"
#include "task_sensor.h"
#include "file_manager.h"
#include "web_pages.h"

static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const static int CONNECTED_BIT = BIT0;

const static char *TAG = "TASK_WIFI";



static OPERATION_t oper = (OPERATION_t){18.5,-22.0,0.,0.,0};

#define HTTP_PORT  80   // or 8080.....

/* Is a character a decimal digit??  */
static bool isDigit(char c)
{
    return ((c >= '0') && (c <= '9'));
}

static int digitVal(char c)
{
    return ((int)c) - (int)'0';
}

static int process_received_buf(char * in_buf, int in_len, char * out_buf, int * out_len);

static void openssl_example_task(void *p)
{
    int ret;

    struct netconn * nc1;
    struct netconn * nc2;

    char recv_buf[OPENSSL_EXAMPLE_RECV_BUF_LEN];

    ESP_LOGI(TAG, "HTTP server netconn create ......");
    nc1 = netconn_new(NETCONN_TCP);
    if (!nc1) {
        ESP_LOGI(TAG, "failed");
        goto failed1;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HHTP server bind to a port......");
    ret = netconn_bind(nc1, IP_ADDR_ANY, HTTP_PORT);
    if (!!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

reconnect:
    ESP_LOGI(TAG, "HTTP server listen at the port......");
    ret = netconn_listen(nc1);
    if (!!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server wait for incoming connection......");
    ret = netconn_accept(nc1, &nc2);
    if (!!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed4;
    }
    if (!nc2) {
        ESP_LOGI(TAG, "failed");
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "HTTP server read message ......");
    do {
        struct netbuf * nb1 = NULL;
        memset(recv_buf, 0, OPENSSL_EXAMPLE_RECV_BUF_LEN);
        ret = netconn_recv(nc2, &nb1);
        if (!!ret || !nb1) {
            break;
        }
        
        int recv_buf_len = 0;
        
        recv_buf_len = netbuf_len(nb1);
        if (recv_buf_len > OPENSSL_EXAMPLE_RECV_BUF_LEN - 1)
        {
            recv_buf_len = OPENSSL_EXAMPLE_RECV_BUF_LEN - 1;
        }
        
        recv_buf_len = netbuf_copy(nb1, recv_buf, recv_buf_len);
        
        netbuf_delete(nb1);
        nb1 = NULL;

        
        static char send_data [2048];
        static int send_bytes = 0;
        
        ESP_LOGI(TAG, "HTTP received %d bytes", recv_buf_len);
        ESP_LOGI(TAG, "HTTP read: %s", recv_buf);
        
        send_bytes = 2048;  // initialise to maximum
        int val = process_received_buf(recv_buf, recv_buf_len, send_data, &send_bytes);
        
        if (val != 0)
        {
            ESP_LOGI(TAG, "Send data len = %d", send_bytes);
            ESP_LOGI(TAG, "Send data: %s", send_data);
            
            ret = netconn_write(nc2, send_data, send_bytes, NETCONN_COPY);
            if (!ret) {
                ESP_LOGI(TAG, "OK");
            } else {
                ESP_LOGI(TAG, "error");
            }
        }
        
        
        break;  // Is this needed? Under what circumstances??
        

    } while (1);

    (void) netconn_close(nc2);

    netconn_delete(nc2);
    nc2 = NULL;
// failed 5 not needed ....
failed4:
    goto reconnect;
// failed 3 not needed..
failed2:
    (void) netconn_delete(nc1);
    nc1 = NULL;
failed1:
    vTaskDelete(NULL);
    return ;
} 

static void openssl_server_init(void)
{
    int ret;
    xTaskHandle openssl_handle;

    ret = xTaskCreate(openssl_example_task,
                      OPENSSL_EXAMPLE_TASK_NAME,
                      OPENSSL_EXAMPLE_TASK_STACK_WORDS,
                      NULL,
                      OPENSSL_EXAMPLE_TASK_PRIORITY,
                      &openssl_handle); 

    if (ret != pdPASS)  {
        ESP_LOGI(TAG, "create task %s failed", OPENSSL_EXAMPLE_TASK_NAME);
    }
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        openssl_server_init();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect(); 
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_conn_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[...]\n", EXAMPLE_WIFI_SSID);
    ESP_ERROR_CHECK( esp_wifi_start() );
}



 
static int process_received_buf(char * in_buf, int in_len, char * out_buf, int * out_len)
{
    int ret = 1;

    if (!in_buf || !out_buf || !out_len)
    {
        return 0;    
    }


    if (strstr(in_buf, "GET ") &&
        strstr(in_buf, " HTTP/1.1")) {
        
        char * getpos = strstr(in_buf, "GET ");  // we already know this is non-null...
        
        
        if (0 == memcmp(&getpos[4], "/delete_all", 11))
        {
            ESP_LOGI(TAG, "Sending delete all page");

            ret = create_web_page_delete_all(out_buf, out_len);

        }
        else if (   (getpos[4] == '/')
             && isDigit(getpos[5]) && isDigit(getpos[6])
             && isDigit(getpos[7]) && isDigit(getpos[8]) )
        {
        
            // The remote browser is trying to "get" a four-digit number

            int pageNumber = 1000*digitVal(getpos[5]) + 100*digitVal(getpos[6]) + 10*digitVal(getpos[7]) + 1*digitVal(getpos[8]);

            ESP_LOGI(TAG, "HTTP get 4-digit message");
            ESP_LOGI(TAG, "HTTP write message");

            ret = create_web_page_file(out_buf, out_len, pageNumber);

        }
        else if (   (getpos[4] == '/') && (getpos[5] == ' ')  )
        {
            // The remote browser has requested "/" (i.e. the root page)


            ESP_LOGI(TAG, "HTTP get matched message");
            ESP_LOGI(TAG, "HTTP write message");

            ret = create_web_page(out_buf, out_len, &oper);

        }
        else
        {
            // 404: file not found
            
            ESP_LOGI(TAG, "Not found");
            ESP_LOGI(TAG, "HTTP write message");

            create_web_page_404(out_buf, out_len);
        
        }
    }
    else if (strstr(in_buf, "POST ") &&
        strstr(in_buf, " HTTP/1.1")) {

        oper.result = 0;  // initialised to failed

        char * i1 = strstr(in_buf, "freq=");
        char * i2 = strstr(in_buf, "ampl=");
        char * i3 = strstr(in_buf, "proceed=Proceed");  // The "Proceed" button was pressed on the Delete All page
        char * i4 = strstr(in_buf, "cancel=Cancel");    // The "Cancel" button was pressed on the Delete All page

        if (i4)
        {
            ;  // Do nothing -- the operation was cancelled
        }
        else if (i3)
        {
            fm_delete_all_files(); // Must do the deletion
        }
        else if (i1 && i2)
        {
            float ff,aa;
            sscanf(&i1[5], "%f", &ff);
            sscanf(&i2[5], "%f", &aa);
            ESP_LOGI(TAG, "%0.6f  ::  %0.6f", ff, aa);

            oper.freq_requested = ff;
            oper.ampl_requested = aa;

            prepare_operation(&oper);

            app_main_task_sensor();
            app_main_do_pwm(&oper);
        }
        ESP_LOGI(TAG, "HTTP write message");

        ret = create_web_page(out_buf, out_len, &oper);
    }


    return ret;
}

