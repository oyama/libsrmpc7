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

static void stop()
{
    current_device = NULL;
}

/*
static void _print_hex(unsigned char *src, int len)
{
    const char *hex = "0123456789abcdef";
    int i;
    char buff[3];
    for (i = 0; i < len; i++) {
        buff[0] = hex[src[i] >> 4];
        buff[1] = hex[src[i]&0x0f];
        buff[2] = '\0';
        printf("%02x ", src[i]);
    }
    printf("\n"); 
}
*/

int main()
{
    srm_handle_t *srm;
    srm_online_record_t record;

    if ((srm = srm_open(SRM_DEVICE_NAME_PC7)) == NULL) {
        fprintf(stderr, "can't open device \"%s\": %s\n", SRM_DEVICE_NAME_PC7, srm_get_error_message());
        return 1;
    }
    current_device = srm;
    signal(SIGINT, stop);
    signal(SIGTRAP, cleanup);

    printf("start online mode\n");
    printf("Power   HR  Cad Speed   Alt  Temp\n");
    while (current_device != NULL) {
        if (srm_get_online_status(srm, &record)) {
            printf("%5u %4u %4u %5.1f %5u %5.1f  %u %u\n",
                record.power,
                record.heart_rate,
                record.cadence,
                (double)(record.speed * 0.1),
                record.altitude,
                (double)(record.temperature * 0.1),
                record.rec1,
                record.marker);
            fflush(stdout);
        }
        sleep(1);
    }    
    printf("shutdown online mode\n");
    srm_close(srm);
    current_device = NULL;
    return 0;
}
