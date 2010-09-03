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

#define PROGRAM_NAME    "srmsync"

#define OUT_FORMAT_TCX  0
#define OUT_FORMAT_TXT  1
#define DEFAULT_SPLIT_THRESHOLD   (120)  /* 2 hour */

#define TCX_HEAD ""  \
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n" \
    "<TrainingCenterDatabase xmlns=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2 http://www.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd\">\n" \
    "  <Activities>\n" \
    "    <Activity Sport=\"Biking\">\n" \
    "      <Id>%04d-%02d-%02dT%02d:%02d:%02dZ</Id>\n"

#define TCX_LAP_HEAD "" \
    "      <Lap StartTime=\"%04d-%02d-%02dT%02d:%02d:%02dZ\">\n" \
    "        <TotalTimeSeconds>%.2f</TotalTimeSeconds>\n" \
    "        <DistanceMeters>%.6f</DistanceMeters>\n" \
    "        <MaximumSpeed>%.6f</MaximumSpeed>\n" \
    "        <Calories>%u</Calories>\n" \
    "        <AverageHeartRateBpm>\n" \
    "          <Value>%u</Value>\n" \
    "        </AverageHeartRateBpm>\n" \
    "        <MaximumHeartRateBpm>\n" \
    "          <Value>%u</Value>\n" \
    "        </MaximumHeartRateBpm>\n" \
    "        <Intensity>Active</Intensity>\n" \
    "        <Cadence>%u</Cadence>\n" \
    "        <TriggerMethod>Manual</TriggerMethod>\n" \
    "        <Track>\n"
#define TCX_TRACKPOINT "" \
    "          <Trackpoint>\n" \
    "            <Time>%04d-%02d-%02dT%02d:%02d:%02dZ</Time>\n" \
    "            <AltitudeMeters>%.3f</AltitudeMeters>\n" \
    "            <DistanceMeters>%.3f</DistanceMeters>\n" \
    "            <HeartRateBpm>\n" \
    "              <Value>%u</Value>\n" \
    "            </HeartRateBpm>\n" \
    "            <Cadence>%u</Cadence>\n" \
    "            <SensorState>Present</SensorState>\n" \
    "            <Extensions>\n" \
    "              <TPX xmlns=\"http://www.garmin.com/xmlschemas/ActivityExtension/v2\">\n" \
    "                <Watts>%u</Watts>\n" \
    "              </TPX>\n" \
    "              <AX xmlns=\"http://oyama.vox.com/xmlschemas/TcxActivityExtension/v1\">\n" \
    "                <Temperature>%.3f</Temperature>\n" \
    "              </AX>\n" \
    "            </Extensions>\n" \
    "          </Trackpoint>\n"
#define TCX_LAP_FOOT "" \
    "        </Track>\n" \
    "        <Extensions>\n" \
    "          <LX xmlns=\"http://www.garmin.com/xmlschemas/ActivityExtension/v2\">\n" \
    "            <AvgWatts>%u</AvgWatts>\n" \
    "          </LX>\n" \
    "        </Extensions>\n" \
    "      </Lap>\n"
#define TCX_FOOT "" \
    "    </Activity>\n" \
    "  </Activities>\n" \
    "</TrainingCenterDatabase>\n"


static srm_handle_t *srm_device = NULL;
static int flag_verbose_mode = 0;


typedef struct output_data_record {
    srm_ride_record_t data;
    struct output_data_record *next;
} output_data_record_t;


typedef struct {
    struct tm timestamp;
    output_data_record_t *records; 
} output_file_t;



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
static int output_txt(output_file_t *file, char *dir)
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
        fprintf(fp, "%5u %4u %4u %5.1f %5u %5.1f  %u:%02u:%02u %6u\r\n",
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


typedef struct file_tcx_activity_lap {
    int total_time_seconds;
    double distance_meters;
    double maximum_speed;
    int calories;
    int average_heart_rate_bpm;
    int maximum_heart_rate_bpm;
    int cadence;
    output_data_record_t *begin;
    output_data_record_t *end;
    struct file_tcx_activity_lap *next;
} file_tcx_activity_lap_t;



static file_tcx_activity_lap_t *new_tcx_activity_lap()
{
    file_tcx_activity_lap_t *lap;

    if ((lap = malloc(sizeof(file_tcx_activity_lap_t))) == NULL) {
        fprintf(stderr, "%s: out of memory\n", PROGRAM_NAME);
        exit(1);
    }
    lap->total_time_seconds = 0;
    lap->distance_meters = 0.0;
    lap->maximum_speed = 0.0;
    lap->calories = 0;
    lap->average_heart_rate_bpm = 0;
    lap->maximum_heart_rate_bpm = 0;
    lap->cadence = 0;
    lap->begin = NULL;
    lap->end   = NULL;
    lap->next  = NULL;
    return lap;
}



static int output_tcx(output_file_t *file, char *dir)
{
    output_data_record_t *cur;
    srm_ride_record_t *data;
    char path[PATH_MAX];
    struct tm *ts = &file->timestamp;
    FILE *fp;
    file_tcx_activity_lap_t *head_lap, *new_lap, *cur_lap, *next_lap;
    int lap_num = -1;
    struct tm gmt_tm;
    time_t gmt_t;
    double lap_dist = 0.0;
    struct stat st;

    snprintf(path, sizeof(path), "%s%s%04d-%02d-%02d-%02d-%02d-%02d.tcx",
             dir, (dir[0] == '\0' ? "" : "/"),
             ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
    if (stat(path, &st) == 0) {
        /* skip exist ride file */
        if (flag_verbose_mode) printf("skip exist ride file: %s\n", path);
        return 0;
    }

    /* create tcx lap object */
    cur = file->records;
    cur_lap = NULL;
    head_lap = NULL;
    while (cur != NULL) {
        data = &cur->data;
        if (lap_num != data->interval) {
            new_lap = new_tcx_activity_lap();
            new_lap->begin = cur;
            if (cur_lap != NULL) {
                cur_lap->next = new_lap;
            }
            cur_lap = new_lap;
            lap_num = data->interval;
        }
        if (head_lap == NULL)
            head_lap = cur_lap;
        cur_lap->end = cur;
        cur_lap->total_time_seconds = mktime(&cur_lap->end->data.timestamp) - mktime(&cur_lap->begin->data.timestamp);
        cur_lap->distance_meters += ((data->speed * 0.1)*1000)/60/60;
        if (cur_lap->maximum_speed < data->speed * 0.1)
            cur_lap->maximum_speed = data->speed * 0.1;
        cur_lap->calories += data->power;
        cur_lap->average_heart_rate_bpm += data->heart_rate;
        if (cur_lap->maximum_heart_rate_bpm < data->heart_rate)
            cur_lap->maximum_heart_rate_bpm = data->heart_rate;
        cur_lap->cadence += data->cadence;

        cur = cur->next;
    }

    if (flag_verbose_mode) printf("%s: create new ride file: %s\n", PROGRAM_NAME, path);
    if ((fp = fopen(path, "w")) == NULL) {
        fprintf(stderr, "%s: can't output file: %s\n", PROGRAM_NAME, path);
        return 0;
    }

    cur_lap = head_lap;
    gmt_t = timelocal(&cur_lap->begin->data.timestamp);
    gmtime_r(&gmt_t, &gmt_tm);
    fprintf(fp, TCX_HEAD, gmt_tm.tm_year+1900, gmt_tm.tm_mon+1, gmt_tm.tm_mday, gmt_tm.tm_hour, gmt_tm.tm_min, gmt_tm.tm_sec);
    while (cur_lap != NULL) {
        /* out lap entity */
        gmt_t = timelocal(&cur_lap->begin->data.timestamp);
        gmtime_r(&gmt_t, &gmt_tm);
        fprintf(fp, TCX_LAP_HEAD,
            gmt_tm.tm_year+1900, gmt_tm.tm_mon+1, gmt_tm.tm_mday,
            gmt_tm.tm_hour, gmt_tm.tm_min, gmt_tm.tm_sec,
            (double)cur_lap->total_time_seconds,
            cur_lap->distance_meters,
            cur_lap->maximum_speed,
            (int)(cur_lap->calories/1000),
            (int)(cur_lap->average_heart_rate_bpm/cur_lap->total_time_seconds),
            cur_lap->maximum_heart_rate_bpm,
            (int)(cur_lap->cadence/cur_lap->total_time_seconds));

        /* out trackpoint */
        cur = cur_lap->begin;
        lap_dist = 0.0;
        while (cur_lap->end != cur) {
            data = &cur->data;
            gmt_t = timelocal(&data->timestamp);
            gmtime_r(&gmt_t, &gmt_tm);
            lap_dist += ((data->speed*0.1)*1000)/60/60;
            fprintf(fp, TCX_TRACKPOINT,
                gmt_tm.tm_year+1900, gmt_tm.tm_mon+1, gmt_tm.tm_mday,
                gmt_tm.tm_hour, gmt_tm.tm_min, gmt_tm.tm_sec,
                (double)data->altitude,
                lap_dist,
                data->heart_rate,
                data->cadence,
                data->power,
                data->temperature*0.1); 
            cur = cur->next;
        }

        fprintf(fp, TCX_LAP_FOOT, cur_lap->calories/cur_lap->total_time_seconds);
        next_lap = cur_lap->next;
        free(cur_lap);
        cur_lap = NULL;
        cur_lap = next_lap;
    }

    fprintf(fp, TCX_FOOT);
    fclose(fp);

    return 1;
}


static int output_file(output_file_t *file, char *out_dir, int type)
{
    if (type == OUT_FORMAT_TXT) {
        return output_txt(file, out_dir);
    }
    else if (type == OUT_FORMAT_TCX) {
        return output_tcx(file, out_dir);
    }
    else {
        return output_tcx(file, out_dir);
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

    while ((block = srm_open_ride_block(srm)) != NULL) {
        if (flag_verbose_mode)
            printf("%s: Open block: %s", PROGRAM_NAME, asctime(&block->datetime));
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
            if (flag_verbose_mode) { printf("."); fflush(stdout); }
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

    srm_device = NULL;
    srm_close(srm);
    return 0;
}
