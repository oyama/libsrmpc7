#ifndef _SRMSYNC_H_
#define _SRMSYNC_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <stdio.h>
#include "srmpc7.h"


#define OUT_FORMAT_TCX  0
#define OUT_FORMAT_TXT  1

#define PROGRAM_NAME "srmsync"
extern srm_handle_t *srm_device;
extern int flag_verbose_mode;


typedef struct output_data_record {
    srm_ride_record_t data;
    struct output_data_record *next;
} output_data_record_t;


typedef struct {
    struct tm timestamp;
    output_data_record_t *records;
} output_file_t;


int srm_sync_output_file_txt(output_file_t *file, char *dir);
int srm_sync_output_file_tcx(output_file_t *file, char *dir);

#ifdef __cplusplus
}
#endif
#endif
