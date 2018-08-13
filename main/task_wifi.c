#include "task_wifi.h"
#include "main.h"


#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"

#include "openssl/ssl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

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


/* Is a character a decimal digit??  */
static bool isDigit(char c)
{
    return ((c >= '0') && (c <= '9'));
}

static int digitVal(char c)
{
    return ((int)c) - (int)'0';
}

static void openssl_example_task(void *p)
{
    int ret;

    SSL_CTX *ctx;
    SSL *ssl;

    int sockfd, new_sockfd;
    socklen_t addr_len;
    struct sockaddr_in sock_addr;

    char recv_buf[OPENSSL_EXAMPLE_RECV_BUF_LEN];

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    const unsigned int cacert_pem_bytes = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    const unsigned int prvtkey_pem_bytes = prvtkey_pem_end - prvtkey_pem_start;   

    ESP_LOGI(TAG, "SSL server context create ......");
    /* For security reasons, it is best if you can use
       TLSv1_2_server_method() here instead of TLS_server_method().
       However some old browsers may not support TLS v1.2.
    */
    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ESP_LOGI(TAG, "failed");
        goto failed1;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set own certification......");
    ret = SSL_CTX_use_certificate_ASN1(ctx, cacert_pem_bytes, cacert_pem_start);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set private key......");
    ret = SSL_CTX_use_PrivateKey_ASN1(0, ctx, prvtkey_pem_start, prvtkey_pem_bytes);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server create socket ......");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket bind ......");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(OPENSSL_EXAMPLE_LOCAL_TCP_PORT);
    ret = bind(sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket listen ......");
    ret = listen(sockfd, 32);
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

reconnect:
    ESP_LOGI(TAG, "SSL server create ......");
    ssl = SSL_new(ctx);
    if (!ssl) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket accept client ......");
    new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
    if (new_sockfd < 0) {
        ESP_LOGI(TAG, "failed" );
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    SSL_set_fd(ssl, new_sockfd);

    ESP_LOGI(TAG, "SSL server accept client ......");
    ret = SSL_accept(ssl);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed5;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server read message ......");
    do {
        memset(recv_buf, 0, OPENSSL_EXAMPLE_RECV_BUF_LEN);
        ret = SSL_read(ssl, recv_buf, OPENSSL_EXAMPLE_RECV_BUF_LEN - 1);
        if (ret <= 0) {
            break;
        }
        
        static char send_data [1024];
        static int send_bytes = 0;
        
        
        ESP_LOGI(TAG, "SSL read: %s", recv_buf);
        if (strstr(recv_buf, "GET ") &&
            strstr(recv_buf, " HTTP/1.1")) {
            
            char * getpos = strstr(recv_buf, "GET ");  // we already know this is non-null...
            
            
            if (0 == memcmp(&getpos[4], "/delete_all", 11))
            {
                ESP_LOGI(TAG, "Sending delete all page");
                create_web_page_delete_all(send_data, 1024);
                
                send_bytes = strlen(send_data);
                
                ret = SSL_write(ssl, send_data, send_bytes);
                
                (void) ret;
                                
            }
            else if (   (getpos[4] == '/')
                 && isDigit(getpos[5]) && isDigit(getpos[6])
                 && isDigit(getpos[7]) && isDigit(getpos[8]) )
            {
            
                // The remote browser is trying to "get" a four-digit number

                int pageNumber = 1000*digitVal(getpos[5]) + 100*digitVal(getpos[6]) + 10*digitVal(getpos[7]) + 1*digitVal(getpos[8]);

                ESP_LOGI(TAG, "SSL get 4-digit message");
                ESP_LOGI(TAG, "SSL write message");
                
                create_web_page_file(send_data, 1024, pageNumber);
                
                send_bytes = strlen(send_data);
                
                ESP_LOGI(TAG, "Send data len = %d", send_bytes);
                ESP_LOGI(TAG, "Send data: %s", send_data);
                
                ret = SSL_write(ssl, send_data, send_bytes);
                if (ret > 0) {
                    ESP_LOGI(TAG, "OK");
                } else {
                    ESP_LOGI(TAG, "error");
                }
            
            }
            else
            {
                // Assume the remote browser has requested "/" (i.e. the root page)


                ESP_LOGI(TAG, "SSL get matched message");
                ESP_LOGI(TAG, "SSL write message");
                
                create_web_page(send_data, 1024, &oper);
                
                send_bytes = strlen(send_data);
                
                ESP_LOGI(TAG, "Send data len = %d", send_bytes);
                ESP_LOGI(TAG, "Send data: %s", send_data);
                
                ret = SSL_write(ssl, send_data, send_bytes);
                if (ret > 0) {
                    ESP_LOGI(TAG, "OK");
                } else {
                    ESP_LOGI(TAG, "error");
                }
            }
            break;
        }
        else if (strstr(recv_buf, "POST ") &&
            strstr(recv_buf, " HTTP/1.1")) {
            
            oper.result = 0;  // initialised to failed
            
            char * i1 = strstr(recv_buf, "freq=");
            char * i2 = strstr(recv_buf, "ampl=");
            char * i3 = strstr(recv_buf, "proceed=Proceed");  // The "Proceed" button was pressed on the Delete All page
            char * i4 = strstr(recv_buf, "cancel=Cancel");    // The "Cancel" button was pressed on the Delete All page
            
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
            ESP_LOGI(TAG, "SSL write message");

            create_web_page(send_data, 1024, &oper);
            
            send_bytes = strlen(send_data);
            
            ESP_LOGI(TAG, "Send data len = %d", send_bytes);
            ESP_LOGI(TAG, "Send data: %s", send_data);
            
            ret = SSL_write(ssl, send_data, send_bytes);
            if (ret > 0) {
                ESP_LOGI(TAG, "OK");
            } else {
                ESP_LOGI(TAG, "error");
            }
            break;
        }
    } while (1);
    
    SSL_shutdown(ssl);
failed5:
    close(new_sockfd);
    new_sockfd = -1;
failed4:
    SSL_free(ssl);
    ssl = NULL;
    goto reconnect;
failed3:
    close(sockfd);
    sockfd = -1;
failed2:
    SSL_CTX_free(ctx);
    ctx = NULL;
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
