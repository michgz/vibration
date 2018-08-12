#include "web_pages.h"

#include <string.h>
#include <stdio.h>
#include "web_pages.h"
#include "main.h"

/* Creates web pages to send     */



#include "esp_err.h"
#include "esp_log.h"

const static char *TAG = "WEB_PAGES";


#define HTTP_HEAD_INITIAL "HTTP/1.1 200 OK\r\n" \
                          "Content-Type: text/html\r\n" \
                          "Content-Length: "   // .. and then fill in the length value




#define OPERATION_DEFAULT (OPERATION_t){18.5,-22.0,18.5,-22.0,0}


const OPERATION_t oper_default = OPERATION_DEFAULT;



static int create_web_page_html(char *, int, int *, OPERATION_t *);
static int create_web_page_html_file(char *, int, int *, int);


const char http_head[] = HTTP_HEAD_INITIAL;

int create_web_page(char * buff, int max_buff_size, OPERATION_t * op)
{
    // To avoid too much moving stuff around, create the HTML portion of the page
    //  in the top half of the buffer.
    //
    // Need to add quite a lot of checking to that.
    // 
    int offset = max_buff_size / 2;

    if (op == NULL)
    {
        op = &oper_default;
    }
    else
    {
        if (op->freq_requested < 1.0)
        {
            op->freq_used = 1.0;
        }
        else if (op->freq_requested > 60.0)
        {
            op->freq_used = 60.0;
        }
        else
        {
            op->freq_used = op->freq_requested;
        }
        
        if (op->ampl_requested < -60.0)
        {
            op->ampl_used = -60.0;
        }
        else if (op->ampl_requested > -20.0)
        {
            op->ampl_used = -20.0;
        }
        else
        {
            op->ampl_used = op->ampl_requested;
        }

    }

    int content_len = 0;
    int html_size = create_web_page_html(&buff[offset], max_buff_size - offset, &content_len, op);

    char * buff_2 = buff;

    memcpy(buff_2, http_head, strlen(http_head));
    buff_2 += (sizeof(http_head) - 1);  //  why the "-1"??
    sprintf (buff_2, "%d\r\n\r\n", content_len);
    buff_2 += strlen(buff_2);

    memmove(buff_2, &buff[offset], strlen(&buff[offset]));
    
    return 1;
}


#define HTML_PAGE       "<html>\r\n" \
                        "<head>\r\n" \
                        "<title>Assistant</title></head><body>\r\n" \
                        "<form action=\"#\" method=\"post\">\r\n" \
                        " Frequency: <input type=\"text\" name=\"freq\" value=\"%0.1f\"> Hz<br>\r\n" \
                        " Amplitude: <input type=\"text\" name=\"ampl\" value=\"%0.1f\"> dB<br>\r\n" \
                        "<input type=\"submit\" value=\"Submit\">\r\n" \
                        "</form>\r\n" \
                        "<a href=\"/0001\">0001</a>\r\n" \
                        "</body>\r\n" \
                        "</html>\r\n" \
                        "\r\n"


const char html_page[] = HTML_PAGE;


static int create_web_page_html(char * buff, int max_size, int * content_length, OPERATION_t * op)
{
    if (op == NULL)
    {
        return 0;
    }
    int line_count = 12;
    sprintf(buff, html_page, op->freq_used, op->ampl_used);
    if (content_length != NULL)
    {
        // Content-Length uses a weird scheme, which is total number of bytes, counting
        // each "\r\n" pair as a single byte, and ignoring the final "\r\n". That's based
        // on Espressif's example. It's possible their example is just wrong..
        *content_length = strlen(buff) - line_count - 1;
    }
    return strlen(buff);
}


int create_web_page_file(char * buff, int max_buff_size, int pageNum)
{
    int offset = max_buff_size / 2;
    
    int content_len = 0;
    int html_size = create_web_page_html_file(&buff[offset], max_buff_size - offset, &content_len, pageNum);

    char * buff_2 = buff;

    memcpy(buff_2, http_head, strlen(http_head));
    buff_2 += (sizeof(http_head) - 1);  //  why the "-1"??
    sprintf (buff_2, "%d\r\n\r\n", content_len);
    buff_2 += strlen(buff_2);

    memmove(buff_2, &buff[offset], strlen(&buff[offset]));
    
    return 1;


}


#define HTML_PAGE_F1    "<html>\r\n" \
                        "<head>\r\n" \
                        "<title>%04d</title></head><body>\r\n" \
                        "<p>1,0.250,0.062,1.030</p>\r\n" \
                        "<a href=\"../\">Home </a>\r\n" \
                        "</body>\r\n" \
                        "</html>\r\n" \
                        "\r\n"


const char html_page_f1[] = HTML_PAGE_F1;


static int create_web_page_html_file(char * buff, int max_size, int * content_length, int pageNum)
{
    int line_count = 8;
    sprintf(buff, html_page_f1, pageNum);
    if (content_length != NULL)
    {
        // Content-Length uses a weird scheme, which is total number of bytes, counting
        // each "\r\n" pair as a single byte, and ignoring the final "\r\n". That's based
        // on Espressif's example. It's possible their example is just wrong..
        *content_length = strlen(buff) - line_count - 1;
    }
    return strlen(buff);
}
