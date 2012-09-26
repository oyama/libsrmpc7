#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "srmpc7.h"


static srm_handle_t *current_device = NULL;

static void cleanup()
{
    fprintf(stderr, "catch signal!\n");
    if (current_device == NULL)
        exit(1);
    srm_close(current_device);
    exit(1);
}



int main()
{
    srm_handle_t *srm;
    srm_ride_block_t *file;
    srm_ride_record_t record;
    int rc, num;



    if ((srm = srm_open(NULL)) == NULL) {
        fprintf(stderr, "can't open device \"%s\": %s\n", SRM_DEVICE_NAME_PC7, srm_get_error_message());
        return 1;
    }
    current_device = srm;
    signal(SIGINT, cleanup);
    signal(SIGTRAP, cleanup);

    num = 1;
    while ((file = srm_open_ride_block(srm)) != NULL) {
        fprintf(stderr, "zero=%u[Hz] slope=%.1f[Hz/Nm] wheel=%u[mm] record=%u int=%.1f[s] name=\"%s\" date=%s",
            file->zero_offset, file->slope, file->wheel, file->length, (double)(file->recording_interval/1000), file->nickname, asctime(&(file->datetime)));

        while ((rc = srm_each_ride_record(file, &record)) > 0) {
            if (record.power == 0 && record.heart_rate == 0 && record.cadence == 0 && record.speed == 0) {
           //     continue;
            }
            printf("%5u %4u %4u %5.1f %5d %5.1f  %u:%02u:%02u\r\n",
                record.power,
                record.heart_rate,
                record.cadence,
                (double)(record.speed * 0.1),
                record.altitude,
                (double)(record.temperature * 0.1),
                record.timestamp.tm_hour,record.timestamp.tm_min, record.timestamp.tm_sec
            );
        }
        if (rc == -1) {
            fprintf(stderr, "cant Write buffer\n");
            break;
        }
        srm_close_ride_block(file);
    }    

    srm_close(srm);
    current_device = NULL;
    return 0;
}
