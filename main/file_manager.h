#ifndef FILE_MANAGER_H_
#define FILE_MANAGER_H_

#include "main.h"

extern void fm_delete_all_files(void);
extern int  fm_get_largest_file_number(void);
extern void fm_init(void);
extern void fm_add_file(void);


extern int  fm_get_file(FILE_STRUCT_t *, int);

#endif // FILE_MANAGER_H_
