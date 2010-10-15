#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <time.h>

#include "srmpc7.h"



static srm_handle_t *current_device = NULL;
static int flag_quarqd_mode = 0;


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


int usage()
{
    printf("Usage: srmonline [-hq] [-i INTERVAL]\n"
           "  -h  show this message.\n"
           "  -q  set quarqd mode.\n"
           "  -i  set recording interval, default 1[sec]\n");
    return 0;
}

int main(int argc, char *argv[])
{
    int ch;
    srm_handle_t *srm;
    srm_online_record_t record;
    int interval = 1;
    int timestamp;

    while ((ch = getopt(argc, argv, "hqi:")) != -1) {
        switch (ch) {
        case 'h':
            exit(usage());
            break;
        case 'q':
            flag_quarqd_mode = 1;
            break;
        case 'i':
            interval = strtol(optarg, NULL, 0);
            if (errno == ERANGE || interval == LONG_MAX || interval == LONG_MIN) {
                fprintf(stderr, "srmonline: cannt set recording interval\n");
                exit(0);
            }
            break;
        case '?':
        default:
            exit(usage());
        }
    }
    argc -= optind;
    argv += optind;

    if ((srm = srm_open(SRM_DEVICE_NAME_PC7)) == NULL) {
        fprintf(stderr, "can't open device \"%s\": %s\n", SRM_DEVICE_NAME_PC7, srm_get_error_message());
        return 1;
    }
    current_device = srm;
    signal(SIGINT, stop);
    signal(SIGTRAP, cleanup);

    fprintf(stderr, "start online mode\n");
    if (!flag_quarqd_mode) printf("Power   HR  Cad Speed   Alt  Temp\n");
    while (current_device != NULL) {
        if (srm_get_online_status(srm, &record)) {
            timestamp = time(NULL);
            if (flag_quarqd_mode) {
                printf("<HeartRate id=h timestamp=%ul. BPM='%u' />\n", timestamp, record.heart_rate);
                printf("<Power id=p timestamp=%ul. watts='%u' />\n", timestamp, record.power);
                printf("<Speed id=s timestamp=%ul. RPM='%.1f'/>\n", timestamp, (double)(record.speed * 0.1));
                printf("<Cadence id=c timestamp=%ul. RPM='%u' />\n", timestamp, record.cadence);
            }
            else {
                printf("%5u %4u %4u %5.1f %5u %5.1f  %u %u\n",
                    record.power,
                    record.heart_rate,
                    record.cadence,
                    (double)(record.speed * 0.1),
                    record.altitude,
                    (double)(record.temperature * 0.1),
                    record.rec1,
                    record.marker);
            }
            fflush(stdout);
        }
        sleep(interval);
    }    
    fprintf(stderr, "shutdown online mode\n");
    srm_close(srm);
    current_device = NULL;
    return 0;
}
