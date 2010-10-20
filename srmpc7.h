#ifndef _SRMPC7_H_
#define _SRMPC7_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ftd2xx.h>


#define SRM_DEVICE_NAME_PC7 "POWERCONTROL 7"
#define SRM_DEVICE_NAME_PC6 "POWERCONTROL 6"

#define SRM_NICKNAME_SIZE   20
#define SRM_PACKET_SIZE     264


/**
 * @struct srm_handle_t
 * @brief Structure for handling SRM PowerControl VI/7 device.
 */
typedef struct {
   FT_HANDLE *handle;     /* FTDI D2XX device handle */
   int ride_blocks;        /* Number of ride data block */
   int readed_file;       /* current block numer */
} srm_handle_t;

/**
 * @struct srm_ride_block_t
 * @brief A structure that ride data block.
 */
typedef struct {
    srm_handle_t *handle;
    struct tm datetime;                   /* Start time of this ride block */
    int recording_interval;               /* Recording interval */
    char nickname[SRM_NICKNAME_SIZE+1];   /* A name of PowerControl */
    double slope;                         /* A slope[Hz/Nm] of this ride block */
    int zero_offset;                      /* A Zero offset[Hz] of this ride block */
    int wheel;                            /* A Wheel size[mm] of this ride block */
    int length;                           /* Number of the records to include in ride block */
    int remind;                           /* current record number */
    unsigned char buffer[SRM_PACKET_SIZE];/* raw SRM packet */
    int buffer_remind;                    /* Position of the raw SRM packet buffer */
} srm_ride_block_t;


/**
 * @struct srm_ride_record_t
 * @brief A structure that ride data record.
 */
typedef struct {
    unsigned int power;      /* Power[W] */
    unsigned int cadence;    /* Cadence[rpm] */
    unsigned int speed;      /* Speed[km/h] x 10 */
    unsigned int heart_rate; /* Heart rate[bpm] */
    int altitude;            /* Altitude[m] */
    int temperature;         /* Temperature[deg] x 10 */
    int interval;            /* Interval section number */
    struct tm timestamp;     /* timestamp of this record */
} srm_ride_record_t;


// a4 b0 00 0f 01 06 02 00 00 00 00 00 00 00 37 01 2c e6 e2 
//                      ^^^^^ ^^ ^^^^^ ^^ ^^^^^ ^^^^^ ^^
//                      Power Ca speed HR alt   temp  ??
typedef struct {
    int rec1;
    unsigned int power;      /* Power[W] */
    unsigned int cadence;    /* Cadence[rpm] */
    unsigned int speed;      /* Speed[km/h] x 10 */
    unsigned int heart_rate; /* Heart rate[bpm] */
    int altitude;            /* Altitude[m] */
    int temperature;         /* Temperature[deg] x 10 */
    int marker;              /* 1=interval on, 0=recovery, 230=? */
} srm_online_record_t;


/**
 * Open the SRM PowerControl device.
 * @param The device name
 * @return The opened device handler. Otherwise, NULL is returned to indicate the error.
 */
srm_handle_t *srm_open(const char *name);
/**
 * Close the SRM Powercontrol device handler.
 * @param The device handler pointer.
 * The memory of the handler is free(3).
 */
int srm_close(srm_handle_t *handle);

/**
 * Open next Ride data block handler.
 * @param the device handler pointer.
 * @return The opened ride data block handler. Otherwise, NULL is returned to indicate the end.
 */
srm_ride_block_t *srm_open_ride_block(srm_handle_t *handle);
/**
 * Close Ride data block handler.
 * @param the block handler pointer.
 * The memory of the handler is free(3).
 */
int srm_close_ride_block(srm_ride_block_t *block);

/**
 * Attempts to read next record from ride data block.
 * @param the block handler pointer.
 * @param the pointer to the data structure to store away the value htat it retrieved.
 * @return If successful, the record number(1..n) is returned. Upon reading end of block, zero is returned. Otherwise, a -1 is returned to indicate the error.
 */
int srm_each_ride_record(srm_ride_block_t *block, srm_ride_record_t *record);


int srm_get_online_status(srm_handle_t *handle, srm_online_record_t *record);


int srm_clear_ride_data(srm_handle_t *handle);

/**
 * Get latest error message.
 * @return error message.
 */
const char *srm_get_error_message();


#ifdef __cplusplus
}
#endif
#endif /* _SRMPC7_H_ */
