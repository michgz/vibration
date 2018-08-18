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


#define OPERATION_DEFAULT (OPERATION_t){18.5,-22.0,18.5,-22.0,0}


const OPERATION_t oper_default = OPERATION_DEFAULT;


static int create_web_page_html(char *, int *, OPERATION_t *);
static int create_web_page_html_file(char *, int *, int);
static int create_web_page_html_delete_all(char * buff, int *buff_size);



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



#define HTML_PAGE_FILE_1    "<html>\r\n" \
                            "<head>\r\n" \
                            "<title>%04d</title></head><body>\r\n"



#define HTML_PAGE_FILE_2    "<a href=\"../\">Home </a>\r\n" \
                            "</body>\r\n" \
                            "</html>\r\n" \
                            "\r\n"



const char html_page_f1[] = HTML_PAGE_FILE_1;
const char html_page_f2[] = HTML_PAGE_FILE_2;



typedef struct {

    int have_started;
    FILE_STRUCT_t theF;
    int step;
    int pageNo;
    int nextStep;
    
} FILE_PAGE_t;

static FILE_PAGE_t theStruct = {0};


// Get the "step" in the file HTML page. The first step is the header, and so on.
// The input buffer must be big enough for any step -- the first one is the 
// biggest.
static int get_step(char * buff, int stepNo, FILE_PAGE_t * fp)
{

    if (stepNo == 0)   // header
    {
        sprintf(buff, html_page_f1, fp->pageNo);
        return 1;
    }
    else if (stepNo == 1)  // date line
    {
        // Print measurement time
        sprintf(buff, "<p>%s</p>\r\n", ctime(&fp->theF.time));
        return 1;
    }
    else if (stepNo < 502)   // 500 data lines
    {
        int i = stepNo - 2;
        sprintf(buff, "<p>%d,%0.4f,%0.4f,%0.4f</p>\r\n", fp->theF.read[i].index,
                                                            fp->theF.read[i].x,
                                                            fp->theF.read[i].y,
                                                            fp->theF.read[i].z);
        return 1;
    }
    else if (stepNo == 502)   // footer
    {
        strcpy(buff, html_page_f2);
        return 1;
    }
    
    return 0;

}




#define HTTP_HEAD_3       "HTTP/1.1 200 OK\r\n" \
                          "Content-Type: text/html\r\n" \
                          "Content-Length: %d\r\n\r\n"

const char http_head_3 [] = HTTP_HEAD_3;


int create_web_page_file(char * buff, int * buff_size, int pageNum)
{
    if (!buff || !buff_size)
    {
        return 0;
    }

    if (*buff_size < strlen(html_page_f1) + strlen(html_page_f2))
    {
        return 0;
    }
    
    char * buff_2;
    int len_to_go = * buff_size;
    
    
    if (!theStruct.have_started)
    {
        ESP_LOGI(TAG, "Getting file...");
    
        if (fm_get_file(&theStruct.theF, pageNum))
        {
            ESP_LOGI(TAG, "Have opened file");
            theStruct.have_started = 1;
            theStruct.step = 0;
            theStruct.pageNo = pageNum;
        }
        else
        {
            return 0 ;  // Bail!
        }
    }
    
    // At this point the file is gotten

    // Add up the length from each step.

    int totalLen = 0;
    int i = 0;
    int val = 0;
    
    do {
        val = get_step(buff, i ++, &theStruct);
        totalLen += strlen(buff);
    
    } while (val != 0);

    ESP_LOGI(TAG, "Have found that the total page length will be %d", totalLen);

    buff_2 = buff;


    // Now we have the total length, can fill it in.

    sprintf(buff, http_head_3, totalLen);
    

    // Now do the steps until we run out of room.
    
    
    int thisLen;
    int lenSoFar = 0;
    
    thisLen = strlen(buff);
    len_to_go -= thisLen;
    buff_2 += thisLen;
    lenSoFar += thisLen;
    
    i = 0;
    
    char localBuf [80];
    
    do {
        val = get_step(localBuf, i, &theStruct);
        if (!val)
        {
            // End! Done.
            theStruct.have_started = 0;
            *buff_size = lenSoFar;            
            return 1;
        }
        thisLen = strlen(localBuf);
        if (thisLen < len_to_go)
        {
            // Have room to put it in.
            strcpy(buff_2, localBuf);
            buff_2 += thisLen;
            len_to_go -= thisLen;
            lenSoFar += thisLen;
            i ++;
        
        }
        else
        {
            // Not room -- wait til next time.
            theStruct.nextStep = i;
            *buff_size = lenSoFar;
            return 2;
            
        }
    
    } while (true);

    return 1;

}




int process_more_buf(char * buff, int * buff_size)
{
    if (!theStruct.have_started)
    {
        return 0;  // shouldn't get here!
    }


    int thisLen;
    int lenSoFar = 0;
    
    int len_to_go = *buff_size;
    char * buff_2 = buff;

    
    int i = theStruct.nextStep;
    
    char localBuf [80];
    int val;
    
    do {
        val = get_step(localBuf, i, &theStruct);
        if (!val)
        {
            // End! Done.
            theStruct.have_started = 0;
            *buff_size = lenSoFar;            
            return 1;
        }
        thisLen = strlen(localBuf);
        if (thisLen < len_to_go)
        {
            // Have room to put it in.
            strcpy(buff_2, localBuf);
            buff_2 += thisLen;
            len_to_go -= thisLen;
            lenSoFar += thisLen;
            i ++;
        
        }
        else
        {
            // Not room -- wait til next time.
            theStruct.nextStep = i;
            *buff_size = lenSoFar;
            return 2;
            
        }
    
    } while (true);

    return 1;

}






int create_web_page_delete_all(char * buff, int * buff_size)
{
    if (!buff || ! buff_size)
    {
        return 0;
    }

    int ret = create_web_page_html_delete_all(buff, buff_size);

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


static int create_web_page_html_delete_all(char * buff, int * buff_size)
{
    if (*buff_size <= strlen(html_page_del))
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

















