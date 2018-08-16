#ifndef WEB_PAGES_H_
#define WEB_PAGES_H_

#include "main.h"

extern int create_web_page(char * buff, int * buff_size, OPERATION_t *);
extern int create_web_page_file(char * buff, int max_buff_size, int);
extern int create_web_page_delete_all(char *, int *);
extern int create_web_page_404(char * out_buf, int * out_buf_len);

#endif // WEB_PAGES_H_
