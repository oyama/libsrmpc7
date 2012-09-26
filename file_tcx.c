#include <sys/stat.h>
#include <sys/syslimits.h>
#include "srmsync.h"


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
    "              <!-- <AX xmlns=\"http://oyama.vox.com/xmlschemas/TcxActivityExtension/v1\">\n" \
    "                <Temperature>%.3f</Temperature>\n" \
    "                <ZeroOffset>%d</ZeroOffset>\n" \
    "              </AX> -->\n" \
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
    lap->total_time_seconds = 1;
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


static void clean_zero_record(output_data_record_t *src)
{
    output_data_record_t *cur;
    srm_ride_record_t *rec;
    srm_ride_record_t *rec1 = NULL, *rec2 = NULL, *rec3 = NULL;

    cur = src;
    while (cur != NULL) {
        rec = &cur->data;
        if (rec->power == 0 && rec->cadence == 0 && rec3 != NULL
            && rec1->power == rec2->power && rec2->power == rec3->power
            && rec1->cadence == rec2->cadence && rec2->cadence == rec3->cadence)
        {
            if (flag_verbose_mode && rec1->power != 0)
                printf("%s: clear dirty record at %s", PROGRAM_NAME, asctime(&(rec->timestamp)));
            rec1->power = rec2->power = rec3->power = 0;
            rec1->cadence = rec2->cadence = rec3->cadence = 0;
        }

        if (rec2 != NULL)
            rec3 = rec2;
        if (rec1 != NULL)
            rec2 = rec1;
        rec1 = rec;
        cur = cur->next;
    }
}


int srm_sync_output_file_tcx(output_file_t *file, char *dir)
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
    double total_dist = 0.0;
    struct stat st;

    snprintf(path, sizeof(path), "%s%s%04d-%02d-%02d-%02d-%02d-%02d.tcx",
             dir, (dir[0] == '\0' ? "" : "/"),
             ts->tm_year+1900, ts->tm_mon+1, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
    if (stat(path, &st) == 0) {
        /* skip exist ride file */
        if (flag_verbose_mode) printf("skip exist ride file: %s\n", path);
        return 0;
    }

    clean_zero_record(file->records);

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
        int t = cur_lap->total_time_seconds ?  cur_lap->total_time_seconds : 1;
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
            (int)(cur_lap->average_heart_rate_bpm/t),
            cur_lap->maximum_heart_rate_bpm,
            (int)(cur_lap->cadence/t));

        /* out trackpoint */
        cur = cur_lap->begin;
        // total_dist = 0.0;
        while (cur_lap->end != cur) {
            data = &cur->data;
            gmt_t = timelocal(&data->timestamp);
            gmtime_r(&gmt_t, &gmt_tm);
            total_dist += ((data->speed*0.1)*1000)/60/60;
            fprintf(fp, TCX_TRACKPOINT,
                gmt_tm.tm_year+1900, gmt_tm.tm_mon+1, gmt_tm.tm_mday,
                gmt_tm.tm_hour, gmt_tm.tm_min, gmt_tm.tm_sec,
                (double)data->altitude,
                total_dist,
                data->heart_rate,
                data->cadence,
                data->power,
                (double)data->temperature*0.1,
                data->zero_offset);
            cur = cur->next;
        }

        fprintf(fp, TCX_LAP_FOOT, cur_lap->calories/t);
        next_lap = cur_lap->next;
        free(cur_lap);
        cur_lap = NULL;
        cur_lap = next_lap;
    }

    fprintf(fp, TCX_FOOT);
    fclose(fp);

    return 1;
}
