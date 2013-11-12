#include <stdio.h>
#include <time.h>
#include <signal.h>

#include "srmpc7.h"


srm_handle_t *srm_device = NULL;


static void cleanup()
{
    if (srm_device == NULL)
        exit(1);
    srm_close(srm_device);
    exit(1);
}


static int usage()
{
    printf("Usage: srmdatetime [hs]\n"
           "  -h  show this message.\n"
           "  -s  set datetime by localtime.\n"
           "  -q  set quiet mode.\n");
    return 0;
}


int main(int argc, char *argv[])
{
    srm_handle_t *srm;
    time_t t;
    struct tm dt, *local_dt;
    int ch;
    int flag_set_datetime = 0;
    int flag_quiet_mode   = 0;

    while ((ch = getopt(argc, argv, "hsq")) != -1) {
        switch (ch) {
        case 'h':
            exit(usage());
            break;
        case 's':
            flag_set_datetime = 1;
            break;
        case 'q':
            flag_quiet_mode = 1;
            break;
        default:
            return usage();
        }
    }

    if ((srm = srm_open(NULL)) == NULL) {
        fprintf(stderr, "can't open device: %s\n", srm_get_error_message());
        return 1;
    }
    srm_device = srm;
    signal(SIGINT, cleanup);
    signal(SIGTRAP, cleanup);


    if (flag_set_datetime) {
        time(&t);
        local_dt = localtime((const time_t *)&t);
        if (!flag_quiet_mode) {
            printf("Set: %02d-%02d-%02d %02d:%02d:%02d\n",
                local_dt->tm_year+1900, local_dt->tm_mon+1, local_dt->tm_mday,
                local_dt->tm_hour, local_dt->tm_min, local_dt->tm_sec);
        }
        if (!srm_set_datetime(srm, local_dt)) {
            fprintf(stderr, "can't set device datetime: %s\n", srm_get_error_message());
            srm_close(srm);
            return 1; 
        }
    }

    if (!flag_quiet_mode) {
        if (!srm_get_datetime(srm, &dt)) {
            fprintf(stderr, "can't lookup device datetime: %s\n", srm_get_error_message());
            srm_close(srm);
            return 1;
        }
        printf("PowerControl: %02d-%02d-%02d %02d:%02d:%02d\n",
            dt.tm_year+1900, dt.tm_mon+1, dt.tm_mday,
            dt.tm_hour, dt.tm_min, dt.tm_sec);
    }

    srm_device = NULL;
    srm_close(srm);
    return 0;
}
