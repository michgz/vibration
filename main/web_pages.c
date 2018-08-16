#include "web_pages.h"

#include <string.h>
#include <stdio.h>
#include "web_pages.h"
#include "main.h"

/* Creates web pages to send     */



#include "esp_err.h"
#include "esp_log.h"

#include "file_manager.h"

const static char *TAG = "WEB_PAGES";



static int add_http_header_response(char * buf, int * buf_len);



#define HTTP_HEAD_INITIAL "HTTP/1.1 200 OK\r\n" \
                          "Content-Type: text/html\r\n" \
                          "Content-Length: "   // .. and then fill in the length value


const char http_head[] = HTTP_HEAD_INITIAL;

#define OPERATION_DEFAULT (OPERATION_t){18.5,-22.0,18.5,-22.0,0}


const OPERATION_t oper_default = OPERATION_DEFAULT;

static int add_on_head(char * buf_to, int * buf_to_len, char * buf_from, int buf_from_len)
{
    if (!buf_to || !buf_to_len || !buf_from)
    {
        return 0;
    }

    if (strlen(http_head) + 6 + 4 + buf_from_len > *buf_to_len)
    {
        // Not enough space. Leave 6 characters for the decimal length (probably more than enough!)
        return 0;
    }

    char * buff_current;

    strcpy(buf_to, http_head);
    
    buff_current = buf_to + strlen(buf_to);
    sprintf(buff_current, "%d\r\n\r\n", buf_from_len);
    buff_current = buff_current + strlen(buff_current);
    
    memcpy(buff_current, buf_from, buf_from_len);
    
    *buf_to_len = (buff_current - buf_to) + buf_from_len;
    buf_to[*buf_to_len] = '\0';   // make sure is null-terminated.

    return 1;
}

static int create_web_page_html(char *, int *, OPERATION_t *);
static int create_web_page_html_file(char *, int, int *, int);
static int create_web_page_html_delete_all(char * buff, int max_size);



int create_web_page(char * buff, int *buff_size, OPERATION_t * op)
{
    if (!buff || ! buff_size)
    {
        return 0;
    }
    
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

    int ret = create_web_page_html(buff, buff_size, op);
    
    if (ret)
    {
        ret = add_http_header_response(buff, buff_size);
    }

    return ret;

}


#define HTML_PAGE_1     "<html>\r\n" \
                        "<head>\r\n" \
                        "<title>Assistant</title></head><body>\r\n" \
                        "<menu><form action=\"#\" method=\"post\">\r\n" \
                        " Frequency: <input type=\"text\" name=\"freq\" value=\"%0.1f\"> Hz<br>\r\n" \
                        " Amplitude: <input type=\"text\" name=\"ampl\" value=\"%0.1f\"> dB<br>\r\n" \
                        "<input type=\"submit\" value=\"Submit\">\r\n" \
                        "</form></menu>\r\n"
                                                
                        
#define HTML_PAGE_2     "<p><a href=\"/%04d\">%04d</a></p>\r\n"


#define HTML_PAGE_3     "<p><a href=\"delete_all\">Delete all</a></p>\r\n" \
                        "</body>\r\n" \
                        "</html>\r\n" \
                        "\r\n"


const char html_page_1[] = HTML_PAGE_1;
const char html_page_2[] = HTML_PAGE_2;
const char html_page_3[] = HTML_PAGE_3;


static int create_web_page_html(char * buff, int * buff_size, OPERATION_t * op)
{
    if (op == NULL)
    {
        return 0;
    }
    int line_count = 12;
    
    char * buff_2 = buff;
    int size_remaining = * buff_size;

    if (size_remaining < strlen(html_page_1))  // Not precisely correct, because of the floating-points substitutions. Close enough tho ...
    {
        return 0;
    }

    sprintf(buff_2, html_page_1, op->freq_used, op->ampl_used);
    size_remaining -= strlen(buff_2);
    if (size_remaining <= 0)
    {
        return 0;
    }
    buff_2 += strlen(buff_2);
    
    
    int max_i = fm_get_largest_file_number();
    
    int i;
    
    for (i = 1; i <= max_i; i ++)
    {
        if (size_remaining < strlen(html_page_2))
        {
            return 0;
        }
    
        sprintf(buff_2, html_page_2, i, i);
        
        line_count ++;

        size_remaining -= strlen(buff_2);
        if (size_remaining <= 0)
        {
            return 0;
        }
        buff_2 += strlen(buff_2);

    }
    
    if (size_remaining < strlen(html_page_2))
    {
        return 0;
    }

    strcpy(buff_2, html_page_3);

    size_remaining -= strlen(buff_2);
    if (size_remaining <= 0)
    {
        return 0;
    }
    buff_2 += strlen(buff_2);
        
    return 1;
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


int create_web_page_delete_all(char * buff, int *  buff_size)
{
    if (!buff || ! buff_size)
    {
        return 0;
    }

    int ret = create_web_page_html_delete_all(buff, *buff_size);
    
    if (ret)
    {
        ret = add_http_header_response(buff, buff_size);
    }

    return ret;

}




#define HTML_PAGE_DEL   "<html>\r\n" \
                        "<head>\r\n" \
                        "<title>Delete All!</title></head><body>\r\n" \
                        "<form action = \"../\" method = \"post\">\r\n" \
                        "This will delete all stored data on the remote device. Are you sure you wish to proceed?<br>\r\n" \
                        "<input type=\"submit\" name=\"proceed\" value=\"Proceed\" />\r\n" \
                        "<input type=\"submit\" name=\"cancel\" value=\"Cancel\" />\r\n" \
                        "</form>\r\n" \
                        "</body>\r\n" \
                        "</html>\r\n" \
                        "\r\n"


const char html_page_del[] = HTML_PAGE_DEL;


static int create_web_page_html_delete_all(char * buff, int max_size)
{
    if (max_size <= strlen(html_page_del))
    {
        return 0;
    }

    strcpy(buff, html_page_del);

    return 1;
    
}




/* -------------------------------------------------------------------------- **
 *  ------------           200: OK response         --------------------------**
 * */
 
 
 
#define HTTP_HEAD_1       "HTTP/1.1 200 OK\r\n" \
                          "Content-Type: text/html\r\n" \
                          "Content-Length: "
                          
// .. then the length value

#define HTTP_HEAD_2       "\r\n\r\n";

const char http_head_1 [] = HTTP_HEAD_1;
const char http_head_2 [] = HTTP_HEAD_2;

// Adds a "200 OK" response header.
static int add_http_header_response(char * buf, int * buf_len)
{
    if (!buf || !buf_len)
    {
        return 0;
    }
    
    char the_len [12];
    
    int current_len = strlen(buf);  // *buf_len is the _maximum_ length, not the current.
    
    if (current_len > 999999)
    {
        // Too ridiculous! Bail out
        return 0;
    }
    
    sprintf(the_len, "%d", current_len);
    
    int len_of_the_len = strlen(the_len);
    
    if ((current_len + strlen(http_head_1) + strlen(http_head_2) + len_of_the_len) >= *buf_len)
    {
        return 0;
    }

    *buf_len = (current_len + strlen(http_head_1) + strlen(http_head_2) + len_of_the_len);

    memmove(buf + (strlen(http_head_1) + strlen(http_head_2) + len_of_the_len), buf, current_len +1); // +1 to get the null termination
    
    memmove(buf + (0),                                    http_head_1, strlen(http_head_1));
    memmove(buf + (strlen(http_head_1)),                  the_len    , len_of_the_len);
    memmove(buf + (strlen(http_head_1) + len_of_the_len), http_head_2, strlen(http_head_2));

    return 1;
}








/* -------------------------------------------------------------------------- **
 *  ------------       404: NOT FOUND response      --------------------------**
 * */


#define HTTP_HEAD_404     "HTTP/1.1 404 Not Found\r\n" \
                          "Content-Type: text/html\r\n" \
                          "Content-Length: 0\r\n\r\n"
                          
const char http_head_404 [] = HTTP_HEAD_404;

int create_web_page_404(char * out_buf, int * out_buf_len)
{
    if (! out_buf || ! out_buf_len)
    {
        return 0;
    }    

    if (*out_buf_len < strlen(http_head_404))
    {
        return 0;
    }
    
    strcpy(out_buf, http_head_404);
    * out_buf_len = strlen(http_head_404);
    
    return 1;
}

















