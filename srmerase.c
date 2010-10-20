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




int main(int argc, char *argv[])
{
    srm_handle_t *srm;
    int rc = 0;

    if ((srm = srm_open(SRM_DEVICE_NAME_PC7)) == NULL) {
        fprintf(stderr, "can't open device \"%s\": %s\n", SRM_DEVICE_NAME_PC7, srm_get_error_message());
        return 1;
    }
    current_device = srm;
    signal(SIGINT, cleanup);
    signal(SIGTRAP, cleanup);

    if (!srm_clear_ride_data(srm)) {
        fprintf(stderr, "can't clear ride data\n");
        rc = 1;
    }

    srm_close(srm);
    current_device = NULL;
    return rc;
}
