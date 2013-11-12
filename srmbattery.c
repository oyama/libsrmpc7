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
    int time_left = -1;


    if ((srm = srm_open(NULL)) == NULL) {
        fprintf(stderr, "can't open device \"%s\": %s\n", SRM_DEVICE_NAME_PC7, srm_get_error_message());
        return 1;
    }
    current_device = srm;
    signal(SIGINT, cleanup);
    signal(SIGTRAP, cleanup);


    if (srm_get_battery_time_left(srm, &time_left) == 0) {
        fprintf(stderr, "can't lookup battery time left\n");
        return 1;
    }
    if (time_left < 0) {
        fprintf(stderr, "can't lookup battery time left\n");
        return 1;
    }
    printf("Battery Time Left=%d[hour]\n", time_left);

    srm_close(srm);
    current_device = NULL;
    return 0;
}
