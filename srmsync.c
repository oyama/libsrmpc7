#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <sys/syslimits.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "srmpc7.h"
#include "srmsync.h"

#define DEFAULT_SPLIT_THRESHOLD   (60)  /* 1 hour */

srm_handle_t *srm_device = NULL;
int flag_verbose_mode = 0;


static void cleanup()
{
    if (srm_device == NULL)
        exit(1);
    srm_close(srm_device);
    exit(1);
}



static output_file_t *new_output_file()
{
    output_file_t *f;

    if ((f = malloc(sizeof(output_file_t))) == NULL) {
        fprintf(stderr, "%s: Out of memory\n", PROGRAM_NAME);
        cleanup();
        exit(1);
    }
    f->records = NULL;
    return f;
}


static void free_output_file(output_file_t *file)
{
    output_data_record_t *cur, *next;

    cur = file->records;
    while (cur != NULL) {
        next = cur->next;
        cur->next = NULL;
        free(cur);
        cur = next;
    }
    file->records = NULL;
    free(file);
    file = NULL;
}


static output_data_record_t *new_output_record(srm_ride_record_t *data)
{
    output_data_record_t *r;

    if ((r = malloc(sizeof(output_data_record_t))) == NULL) {
        fprintf(stderr, "%s: Out of memory\n", PROGRAM_NAME);
        cleanup();
        exit(1);
    }
    memmove(&r->data, data, sizeof(srm_ride_record_t));
    r->next = NULL;
    return r; 
}


/* .txt format */
int srm_sync_output_file_txt(output_file_t *file, char *dir)
{
    output_data_record_t *cur;
    srm_ride_record_t *data;
    char path[PATH_MAX];
    struct tm *ts = &file->timestamp;
    FILE *fp;
    int num = 1;
    snprintf(path, sizeof(path), "%s%s%04d-%02d-%02dT%02d:%02d:%02d.txt",
             dir, (dir[0] == '\0' ? "" : "/"),
             ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
    if ((fp = fopen(path, "w")) == NULL) {
        fprintf(stderr, "%s: can't output file: %s\n", PROGRAM_NAME, path);
        return 0;
    }
    cur = file->records;
    while (cur != NULL) {
        data = &cur->data;
        if (data->power == 0 && data->heart_rate == 0 && data->cadence == 0 && data->speed == 0) {
            cur = cur->next;
            continue;
        }
        fprintf(fp, "%5u %4u %4u %5.1f %5d %5.1f  %u:%02u:%02u %6u\r\n",
               data->power,
               data->heart_rate,
               data->cadence,
               (double)(data->speed * 0.1),
               data->altitude,
               (double)(data->temperature * 0.1),
               data->timestamp.tm_hour,data->timestamp.tm_min, data->timestamp.tm_sec,
               num++);
        cur = cur->next;
    }
    fclose(fp);

    return 1;
}



static int output_file(output_file_t *file, char *out_dir, int type)
{
    if (type == OUT_FORMAT_TXT) {
        return srm_sync_output_file_txt(file, out_dir);
    }
    else if (type == OUT_FORMAT_TCX) {
        return srm_sync_output_file_tcx(file, out_dir);
    }
    else {
        return srm_sync_output_file_tcx(file, out_dir);
    }
}


static int sync_srm_to_directory(srm_handle_t *srm, char *out_dir, int type, int interval)
{
    srm_ride_block_t *block;
    srm_ride_record_t record;
    time_t last_record = 0;
    int rc;
    output_file_t *file = NULL;
    output_data_record_t *rec = NULL;
    output_data_record_t *last_rec = NULL;
    time_t start_time = 0;
    int record_counter = 0;

    while ((block = srm_open_ride_block(srm)) != NULL) {
        if (flag_verbose_mode) {
            record_counter = 0;
            printf("zero=%u[Hz] slope=%.1f[Hz/Nm] wheel=%u[mm] record=%u int=%.1f[s] name=\"%s\" date=%s",
                block->zero_offset, block->slope, block->wheel, block->length,
                (double)(block->recording_interval/1000), block->nickname, asctime(&(block->datetime)));
        }
        start_time = mktime(&block->datetime);
        if (start_time - last_record > interval*60) {
            if (file != NULL) {
                if (flag_verbose_mode) printf("\n");
                output_file(file, out_dir, type);
                free_output_file(file);
            }
            file = new_output_file();
            memmove(&file->timestamp, &block->datetime, sizeof(struct tm));
        }
        while ((rc = srm_each_ride_record(block, &record)) > 0) {
            if (flag_verbose_mode && (record_counter++ % 60) == 0) {
                printf(".");
                fflush(stdout);
            }
            rec = new_output_record(&record);
            last_record = mktime(&record.timestamp);
            if (file->records == NULL) {
                file->records = rec;
            }
            if (last_rec != NULL)
                last_rec->next = rec;
            last_rec = rec;
        }
        if (flag_verbose_mode) printf("\n");
    }
    if (file != NULL) {
        output_file(file, out_dir, type);
        free_output_file(file);
    }

    return 1;
}


static void report_srm_battery_time_left(srm_handle_t *srm)
{
    int time_left = -1;

    if (srm_get_battery_time_left(srm, &time_left) == 0) {
        fprintf(stderr, "%s: can't lookup battery time left\n", PROGRAM_NAME);
        return;
    }
    if (time_left < 0) {
        fprintf(stderr, "%s: can't lookup battery time left\n", PROGRAM_NAME);
        return;
    }
    printf("Battery Time Left=%d[hour]\n", time_left);
    return;
}


static int usage()
{
    printf("Usage: %s [-hv] [-t FILE_TYPE] [-i SPLIT_THRESHOLD] /path/to/directory\n"
           "  -h   show this message.\n"
           "  -t   set output file format, 'tcx' or 'txt', default 'tcx'.\n"
           "  -i   split log file inactivity threshold, default 60[min].\n"
           "  -v   set verbose mode.\n",
           PROGRAM_NAME);
    return 0;
}


int main(int argc, char *argv[])
{
    int ch;
    int flag_file_type     = OUT_FORMAT_TCX;
    int flag_inactive_time = DEFAULT_SPLIT_THRESHOLD;
    char *out_directory;
    struct stat dstat;
    srm_handle_t *srm;

    while ((ch = getopt(argc, argv, "hvt:i:")) != -1) {
        switch (ch) {
        case 'h':
            exit(usage());
            break;
        case 'v':
            flag_verbose_mode = 1;
            break;
        case 't':
            if (strcmp("tcx", optarg) == 0) {
                flag_file_type = OUT_FORMAT_TCX;
            }
            else if (strcmp("txt", optarg) == 0) {
                flag_file_type = OUT_FORMAT_TXT;
            }
            else {
                fprintf(stderr, "%s: unknown file format: %s, please use 'tcx' or 'txt'\n", PROGRAM_NAME, optarg);
                exit(1);
            }
            break;
        case 'i':
            flag_inactive_time = strtol(optarg, NULL, 0);
            if (errno == ERANGE || flag_inactive_time == LONG_MAX || flag_inactive_time == LONG_MIN) {
                fprintf(stderr, "%s: cannot set inactive shreshold\n", PROGRAM_NAME);
                exit(1);
            }
            break;
        case '?':
        default:
            return usage();
        }
    }
    argc -= optind;
    argv += optind;
    out_directory = argv[0];
    if (out_directory == NULL || out_directory[0] == '\0')
        return usage();
    if (stat(out_directory, &dstat) != 0 || !S_ISDIR(dstat.st_mode)) {
        fprintf(stderr, "%s: No such directory\n", out_directory);
        exit(1);
    }


    if ((srm = srm_open(SRM_DEVICE_NAME_PC7)) == NULL) {
        fprintf(stderr, "can't open device \"%s\": %s\n", SRM_DEVICE_NAME_PC7, srm_get_error_message());
        return 1;
    }
    srm_device = srm;
    signal(SIGINT, cleanup);
    signal(SIGTRAP, cleanup);

    sync_srm_to_directory(srm, out_directory, flag_file_type, flag_inactive_time);
    report_srm_battery_time_left(srm);

    srm_device = NULL;
    srm_close(srm);
    return 0;
}
