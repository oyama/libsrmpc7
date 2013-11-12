#include "srmpc7.h"


#define SRM_CMD_SIZE              7
#define SRM_CMD_RESPONSE_SIZE     9
#define SRM_RIDE_FILE_HEAD_SIZE   54
#define SRM_DEVICE_LATENCY_TIMER  16

#define SRM_WIRELESS_DEVICE_DESCRIPTION "POWERCONTROL "


static const unsigned char SRM_CMD_HEAD[2] = {0xa4, 0xb0};

static const unsigned char SRM_CMD_HELLO[2]             = {0x01, 0x01};
static const unsigned char SRM_CMD_GET_ONLINE_STATUS[2] = {0x01, 0x06};
//static const unsigned char SRM_CMD_UNKNOWN[2]         = {0x01, 0x0C}; // 00 00 00 00

//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x01}; // 00 00
static const unsigned char SRM_CMD_GET_ZERO_OFFSET[2]   = {0x02, 0x02}; // int[2], int[2]
static const unsigned char SRM_CMD_GET_CIRCUMFERENCE[2] = {0x02, 0x03}; // 08 55
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x04}; // 01 00 7A 00 A6 00 C6 00 E8 01 08
static const unsigned char SRM_CMD_GET_INITIAL[2]       = {0x02, 0x05}; // initial[20];
//static const unsigned char SRM_CMD_GET_INTERVAL[2]    = {0x02, 0x06}; // 03 E8 = 1000
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x07}; // 01 01
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x08}; // 14 14 03
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x09}; // 01
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x0A}; // 01
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x0B}; // 01 00 78
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x0C}; // 03
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x0D}; // 00 00 00 5B 00 18 AA 63 00 03
static const unsigned char SRM_CMD_GET_DATETIME[2]      = {0x02, 0x0E}; // dd mm YY YY HH MM SS
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x0F}; // 01
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x10}; // 01 01 01
//static const unsigned char SRM_CMD_GET_WIRELESS_SETTING[2] = {0x02, 0x11}; // PM.SN[2] SP.SN[2] HR.SN[2]
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x12}; // 02
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x13}; // 02
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x14}; // 02
//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x15}; // 02

//static const unsigned char SRM_CMD_GET_UNKNOWN[2]     = {0x02, 0x17}; // 02 46 50

static const unsigned char SRM_CMD_HOW_MANY[2]         = {0x04, 0x01};
static const unsigned char SRM_CMD_NEXT_RIDE_FILE[2]   = {0x04, 0x02};
//
static const unsigned char SRM_CMD_GET_RIDE_RECORDS[2] = {0x04, 0x04};
static const unsigned char SRM_CMD_RETRY_GET_RIDE_RECORDS[2] = {0x04, 0x05};
//
static const unsigned char SRM_CMD_CLEAR_RIDE_DATA[2]  = {0x04, 0x07};
static const unsigned char SRM_CMD_BATTERY_TIME_LEFT[2] = {0x04, 0x08}; // 3F 3F 05 67


static const unsigned char SRM_RESPONSE_HELLO[] = {0xa4, 0xb0, 0x00, 0x05, 0x01, 0x01, 0xa6, 0x15, 0xa2};

static char srm_error_message[1024];
static int DEBUG_RECORD = 0;
//static int INVALID_CHECKSUM = 0;



static void _print_hex(const char *label, unsigned char *src, int len)
{
    const char *hex = "0123456789abcdef";
    int i;
    char buff[3];
    fprintf(stderr, "%s = {\n", label);
    for (i = 0; i < len; i++) {
        buff[0] = hex[src[i] >> 4];
        buff[1] = hex[src[i]&0x0f];
        buff[2] = '\0';
        fprintf(stderr, "%02x ", src[i]);
        if (i != 0 && i % 16 == 0)
            fprintf(stderr, "\n");
    }
    fprintf(stderr, "}\n");
}



static unsigned char get_packet_checksum(unsigned char *in, size_t l)
{
    unsigned char sum = 0;
    int i;
    for (i = 0; i < l; i++) {
        sum ^= in[i];
    }
    return sum;
}


static int is_valid_packet(unsigned char *packet, size_t l)
{
    unsigned char sum = get_packet_checksum(packet, l-1);
    return sum == packet[l-1];
}


static unsigned char *srm_create_message(unsigned char *dist, const unsigned char *msg)
{
    dist[0] = SRM_CMD_HEAD[0];
    dist[1] = SRM_CMD_HEAD[1];
    dist[2] = 0x00;
    dist[3] = 0x03;
    dist[4] = msg[0];
    dist[5] = msg[1];
    dist[6] = get_packet_checksum(dist, 6);
    return dist;
}


static unsigned char *srm_create_message_with_body(unsigned char *dist, const unsigned char *command, unsigned char *body, size_t body_l)
{
    int i = 0, j;

    dist[i] = SRM_CMD_HEAD[0];
    dist[++i] = SRM_CMD_HEAD[1];
    dist[++i] = 0x00;
    dist[++i] = 2 + body_l+1;
    dist[++i] = command[0];
    dist[++i] = command[1];
    for (j = 0; j < body_l; j++) {
        dist[++i] = body[j];
    }
    dist[++i] = get_packet_checksum(dist, 6 + body_l);
    return dist;
}


static int get_num_of_ride_blocks(unsigned char *in, size_t in_size)
{
    int num;

    if (in_size < 9)
        return -1;
    num  = in[6] << 8 | in[7];
    return num;
}


static int get_ride_block_datetime(struct tm *t, unsigned char *in)
{
    time_t time;
    struct tm *t2;
    t->tm_mday = in[8];
    t->tm_mon = in[9] - 1;
    t->tm_year = (in[10] << 8 | in[11]) - 1900;
    t->tm_hour = in[12];
    t->tm_min = in[13];
    t->tm_sec = in[14];
    t->tm_isdst = 0;

    time = mktime(t);
    t2 = localtime(&time);
    memmove(t, t2, sizeof(struct tm)); 
    return 1;
}


static int wait_device_writable(srm_handle_t *handle)
{
    FT_HANDLE fh = *(handle->handle);
    DWORD rxBytes, txBytes, eventDWord;
    int result = -1;

    while (FT_GetStatus(fh, &rxBytes, &txBytes, &eventDWord) == FT_OK) {
        if (txBytes == 0)
            break;
    }
    return result;
}


static int wait_device_readable(srm_handle_t *handle)
{
    FT_HANDLE fh = *(handle->handle);
    DWORD rxBytes, txBytes, eventDWord;
    int result = -1;

    while (FT_GetStatus(fh, &rxBytes, &txBytes, &eventDWord) == FT_OK) {
        if (rxBytes > 0)
            break;
    }
    return result;
}


static size_t srm_msg_exchange(srm_handle_t *handle, const unsigned char *cmd,
                               unsigned char *body, size_t body_size,
                               unsigned char *buffer, size_t buff_size, size_t *out_size)
{
    FT_STATUS st;
    FT_HANDLE fh = *(handle->handle);
    unsigned char msg[SRM_CMD_SIZE];
    unsigned char buff[SRM_PACKET_SIZE];
    DWORD l = 0;
    unsigned char *long_msg;

    wait_device_writable(handle);
    if (body_size == 0) {
        st = FT_Write(fh, srm_create_message(msg, cmd), sizeof(msg), &l);
    }
    else {
        if ((long_msg = malloc(body_size+7)) == NULL) {
            strcpy(srm_error_message, "Out of memory");    
            return 0;
        }
        srm_create_message_with_body(long_msg, cmd, body, body_size);
        st = FT_Write(fh, long_msg, body_size+7, &l);
        free(long_msg);
    }
    if (st != FT_OK) {
        fprintf(stderr, "can't FT_Write\n");
        return 0;
    }

    st = FT_Read(fh, buffer, buff_size, &l);
    if (st != FT_OK) {
        fprintf(stderr, "can't FT_Read\n");
        return 0;
    }
    *out_size = l;

    st = FT_Read(fh, buff, 0, &l);
    if (st != FT_OK) {
        fprintf(stderr, "clear read buffer FT_Read is not FT_OK\n");
        return 0;
    }
    if (l != 0) {
        fprintf(stderr, "clear read buffer FT_Read is not l != 0\n");
        return 0;
    }

    while (memcmp(cmd, SRM_CMD_GET_RIDE_RECORDS, 2) == 0
        && !is_valid_packet(buffer, *out_size))
    {
        fprintf(stderr, "retry get ride records\n");
        /* retry error packet */
        wait_device_writable(handle);    
        st = FT_Write(fh, srm_create_message(msg, SRM_CMD_RETRY_GET_RIDE_RECORDS), sizeof(msg), &l);
        if (st != FT_OK) {
            fprintf(stderr, "can't FT_Write\n");
            return 0;
        }
    
        wait_device_readable(handle);    
        st = FT_Read(fh, buffer, buff_size, &l);
        if (st != FT_OK) {
            fprintf(stderr, "can't FT_Read\n");
            return 0;
        }
        *out_size = l;
    
        /* clear read buffer */
        st = FT_Read(fh, buff, 0, &l);
        if (st != FT_OK) {
            fprintf(stderr, "clear read buffer FT_Read is not FT_OK\n");
            return 0;
        }
        if (l != 0) {
            fprintf(stderr, "clear read buffer FT_Read is not l != 0\n");
            return 0;
        }
    }
    return *out_size;
}


static int init_handle(srm_handle_t *handle)
{
    FT_STATUS st;
    FT_HANDLE fh;
    unsigned char buff[SRM_CMD_RESPONSE_SIZE];
    size_t rc, len;

    fh = *(handle->handle);

    st = FT_SetBaudRate(fh, FT_BAUD_38400);
    if (st != FT_OK) {
        strcpy(srm_error_message, "can't set baud rate");
        return 0;
    }
    st = FT_SetFlowControl(fh, FT_FLOW_NONE, 0, 0);
    if (st != FT_OK) {
        strcpy(srm_error_message, "can't set flow control");
        return 0;
    }
    st = FT_SetTimeouts(fh, 1000, 500);
    if (st != FT_OK) {
        strcpy(srm_error_message, "can't set timeouts");
        return 0;
    }
    st = FT_SetLatencyTimer(fh, SRM_DEVICE_LATENCY_TIMER);
    if (st != FT_OK) {
        strcpy(srm_error_message, "can't set latency timer");
        return 0;
    }
    st = FT_SetDataCharacteristics(fh, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    if (st != FT_OK) {
        strcpy(srm_error_message, "can't set data characteristics");
        return 0;
    }
    st = FT_SetUSBParameters(fh, 64, 64);
    if (st != FT_OK) {
        strcpy(srm_error_message, "can't set data characteristics");
        return 0;
    }

    wait_device_writable(handle);    
    rc = srm_msg_exchange(handle, SRM_CMD_HELLO, NULL, 0, buff, sizeof(buff), &len);
    wait_device_writable(handle);    
    rc = srm_msg_exchange(handle, SRM_CMD_HELLO, NULL, 0, buff, sizeof(buff), &len);
    if (rc <= 0) {
        strcpy(srm_error_message, "can't exchange cmd HELLO");
        return 0;
    }

    if (memcmp(buff, SRM_RESPONSE_HELLO, sizeof(SRM_RESPONSE_HELLO)-3) != 0) {
        strcpy(srm_error_message, "is not cmd HELLO response");
        return 0;
    }

    rc = srm_msg_exchange(handle, SRM_CMD_HOW_MANY, NULL, 0, buff, sizeof(buff), &len);
    if (rc <= 0) {
        strcpy(srm_error_message, "is not HOW_MANY response");
        return 0;
    }
    handle->ride_blocks  = get_num_of_ride_blocks(buff, len);
    handle->readed_file = 0;
    return 1;
}


static int clear_handle(srm_handle_t *handle)
{
    if (handle == NULL)
        return 1;
    
    free(handle->handle);
    handle->handle = NULL;
    free(handle);
    handle = NULL;
    return 1;
}



srm_handle_t *srm_open(const char *name)
{
    FT_STATUS st;
    DWORD nd;
    FT_DEVICE_LIST_INFO_NODE *dev_info;
    int i;
    char dev_name[64];
    FT_HANDLE *ft;
    srm_handle_t *handle;

    if ((st = FT_CreateDeviceInfoList(&nd)) != FT_OK) {
        strcpy(srm_error_message, "can't create D2XX device info list");
        return NULL;
    }
    if (nd <= 0) {
        strcpy(srm_error_message, "can't locate D2XX device");
        return NULL;
    }
    if ((dev_info = malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*nd)) == NULL) {
        strcpy(srm_error_message, "Out of memory");    
        return NULL;
    }
    if ((st = FT_GetDeviceInfoList(dev_info, &nd)) != FT_OK) {
        strcpy(srm_error_message, "can't lookup D2XX device list");    
        return NULL;
    }

    dev_name[0] = '\0';
    for (i = 0; i < nd; i++) {
        if (dev_info[i].Description == NULL)
            continue;
        if (strncmp(dev_info[i].Description, SRM_WIRELESS_DEVICE_DESCRIPTION, strlen(SRM_WIRELESS_DEVICE_DESCRIPTION)) == 0)
        {
            strncpy(dev_name, dev_info[i].Description, sizeof(dev_name));
            break;
        }
    }
    free(dev_info);

    if (dev_name[0] == '\0') {
        strcpy(srm_error_message, "can't locate SRM Wireless device");
        return NULL;
    }

    if ((ft = malloc(sizeof(FT_HANDLE))) == NULL) {
        strcpy(srm_error_message, "Out of memory");    
        return NULL;
    }
    st = FT_OpenEx(dev_name, FT_OPEN_BY_DESCRIPTION, ft);
    if (st != FT_OK) {
        strcpy(srm_error_message, "can't open by device name");
        free(ft);
        return NULL;
    }

    if ((st = FT_ResetDevice(*ft)) != FT_OK) {
        strcpy(srm_error_message, "can't reset device");
        free(ft);
        return NULL;
    }
    if ((handle = malloc(sizeof(srm_handle_t))) == NULL) {
        strcpy(srm_error_message, "Out of memory");    
        free(ft);
        return NULL;
    }
    handle->handle      = ft;
    handle->readed_file = 0;
    handle->ride_blocks = 0;
    if (init_handle(handle) == 0) {
        clear_handle(handle);
        return NULL;
    }
    return handle;
}


int srm_close(srm_handle_t *handle)
{
    if (handle == NULL)
        return 1;
    if (handle->handle != NULL) {
        FT_Close(*(handle->handle));
    }
    clear_handle(handle);
    return 1;
}


srm_ride_block_t *srm_open_ride_block(srm_handle_t *handle)
{
    srm_ride_block_t *fh;
    unsigned char buff[SRM_RIDE_FILE_HEAD_SIZE];
    size_t rc, len;

    if (handle->readed_file >= handle->ride_blocks) {
        return NULL;
    }

    rc = srm_msg_exchange(handle, SRM_CMD_NEXT_RIDE_FILE, NULL, 0, buff, sizeof(buff), &len);
    if (rc <= 0) {
        strcpy(srm_error_message, "can't exchange cmd NEXT_RIDE_FILE");
        return NULL;
    }
    if ((fh = malloc(sizeof(srm_ride_block_t))) == NULL) {
        strcpy(srm_error_message, "Out of memory");
        return NULL;
    }
    fh->handle = handle;
    get_ride_block_datetime(&fh->datetime, buff);
    fh->recording_interval = buff[15] << 8 | buff[16];

    memmove(fh->nickname, buff+17, 20);
    fh->nickname[20]  = '\0';
    fh->slope         = (double)((buff[37] << 8 | buff[38]) * 0.1);
    fh->zero_offset   = buff[39] << 8 | buff[40];
    fh->wheel         = buff[45] << 8 | buff[46];
    fh->remind        = 0;
    fh->length        =  buff[47] << 8 | buff[48];
    fh->buffer_remind = 0;
    handle->readed_file++;
    return fh;
}


int srm_close_ride_block(srm_ride_block_t *fh)
{
    if (fh == NULL)
        return 1;
    free(fh);
    fh = NULL;
    return 1;
}


int srm_each_ride_record(srm_ride_block_t *fh, srm_ride_record_t *record)
{
    size_t rc, len;
    unsigned char *cur;
    time_t ts;
    struct tm *timestamp;
    int packet_id;

    if (fh->remind >= fh->length) {
        return 0;
    }
    if (fh->buffer_remind == 0) {
        rc = srm_msg_exchange(fh->handle, SRM_CMD_GET_RIDE_RECORDS, NULL, 0, fh->buffer, SRM_PACKET_SIZE, &len);
        if (rc == 0) {
            return -1;
        }
        if (fh->buffer[0] == 0xa4 && fh->buffer[1] == 0xb0) {
            fh->buffer_remind += 2 + 2 + 4;
        }
        else {
            fprintf(stderr, "require retry packet, at packet %d\n", fh->remind+1);
        }
        packet_id = (fh->buffer[6] << 8) | fh->buffer[7];
        if ((fh->remind/ 16 + 1) != packet_id) {
            fprintf(stderr, "invalid packet id=%d, at packet %d\n", packet_id, fh->remind+1);
        }
    }
    cur = fh->buffer + fh->buffer_remind;
    if (DEBUG_RECORD) {
        _print_hex("DEBUG buffer", fh->buffer, len);
        fprintf(stderr, "buffer remind: %d\n", fh->buffer_remind);
        _print_hex("DEBUG", cur, 16);
    }
    record->power       = (cur[0] << 8) | cur[1];
    record->cadence     = cur[2];
    record->speed       = (cur[3] << 8) | cur[4];
    record->heart_rate  = cur[5];
    record->altitude    = (short)((cur[6] << 8) | cur[7]);
    record->temperature = (short)((cur[8] << 8) | cur[9]);
    record->interval    = cur[10];

    ts = mktime(&fh->datetime) + fh->remind;
    timestamp = localtime(&ts);
    memmove(&record->timestamp, timestamp, sizeof(struct tm)); 

    record->slope       = fh->slope;
    record->zero_offset = fh->zero_offset;

    fh->buffer_remind += 16;
    if (fh->buffer_remind >= SRM_PACKET_SIZE)
        fh->buffer_remind = 0;
    fh->remind += 1;

    return fh->remind;
}



int srm_get_online_status(srm_handle_t *handle, srm_online_record_t *record)
{
    size_t rc, out_len;
    unsigned char buff[19];

    rc = srm_msg_exchange(handle, SRM_CMD_GET_ONLINE_STATUS, NULL, 0, buff, sizeof(buff), &out_len);
    if (rc <= 0) {
        strcpy(srm_error_message, "can't exchange cmd GET_ONLINE_STATUS");
        return 0;
    }
    if (!is_valid_packet(buff, rc)) {
        strcpy(srm_error_message, "invalid packet GET_ONLINE_STATUS");
        return 0;
    }

    record->rec1        = buff[6];
    record->power       = (buff[7] << 8) | buff[8];
    record->cadence     = buff[9];
    record->speed       = (buff[10] << 8) | buff[11];
    record->heart_rate  = buff[12];
    record->altitude    = (short)((buff[13] << 8) | buff[14]);
    record->temperature = (short)((buff[15] << 8) | buff[16]);
    record->marker      = buff[17];

    return 1;
}


int srm_get_zero_offset(srm_handle_t *handle, int *zero, int *torque)
{
    size_t rc, out_len;
    unsigned char buff[11];

    rc = srm_msg_exchange(handle, SRM_CMD_GET_ZERO_OFFSET, NULL, 0, buff, sizeof(buff), &out_len);
    if (rc <= 0) {
        strcpy(srm_error_message, "can't exchange cmd SRM_CMD_GET_ZERO_OFFSET");
        return 0;
    }
    if (!is_valid_packet(buff, rc)) {
        strcpy(srm_error_message, "invalid packet SRM_CMD_GET_ZERO_OFFSET");
        return 0;
    }

    *zero = (buff[6] << 8) | buff[7];
    *torque = (buff[8] << 8) | buff[9];
    return 1;
}


int srm_clear_ride_data(srm_handle_t *handle)
{
    size_t rc, out_len;
    unsigned char buff[64];
 
    rc = srm_msg_exchange(handle, SRM_CMD_CLEAR_RIDE_DATA, NULL, 0, buff, sizeof(buff), &out_len);
    if (rc <= 0) {
        strcpy(srm_error_message, "can't exchange cmd SRM_CMD_CLEAR_RIDE_DATA");
        return 0;
    }
    if (!is_valid_packet(buff, rc)) {
        strcpy(srm_error_message, "invalid packet SRM_CMD_CLEAR_RIDE_DATA");
        return 0;
    }

    return 1;
}


int srm_get_battery_time_left(srm_handle_t *handle, int *hour)
{
    size_t rc, l;
    unsigned char buff[64];

    rc = srm_msg_exchange(handle, SRM_CMD_BATTERY_TIME_LEFT, NULL, 0, buff, sizeof(buff), &l);
    if (rc <= 0) {
        strcpy(srm_error_message, "can't exchange cmd SRM_CMD_BATTERY_TIME_LEFT");
        return 0;
    }
    if (!is_valid_packet(buff, rc)) {
        strcpy(srm_error_message, "invalid packet SRM_CMD_BATTERY_TIME_LEFT");
        return 0;
    }
    // buff[6], buff[7] unknown
    *hour = (buff[8] << 8) | buff[9];
    return 1;
}



int srm_get_datetime(srm_handle_t *handle, struct tm *datetime)
{
    int rc;
    size_t l;
    unsigned char buff[64];
    time_t time;
    struct tm *t;

    rc = srm_msg_exchange(handle, SRM_CMD_GET_DATETIME, NULL, 0, buff, sizeof(buff), &l);
    if (rc <= 0) {
        strcpy(srm_error_message, "can't exchange cmd SRM_CMD_GET_DATETIME");
        return 0;
    }

    datetime->tm_mday  = buff[6];
    datetime->tm_mon   = buff[7] - 1;
    datetime->tm_year  = (buff[8] << 8 | buff[9]) - 1900;
    datetime->tm_hour  = buff[10];
    datetime->tm_min   = buff[11];
    datetime->tm_sec   = buff[12];
    datetime->tm_isdst = 0;

    time = mktime(datetime);
    t = localtime(&time);
    memmove(datetime, t, sizeof(struct tm));

    return 1; 
}


const char *srm_get_error_message()
{
    return srm_error_message;
}


int srm_set_datetime(srm_handle_t *handle, struct tm *datetime)
{
    unsigned char srm_time[7];
    unsigned int year = datetime->tm_year + 1900;
    unsigned char buff[64];
    size_t l;

    srm_time[0] = datetime->tm_mday;
    srm_time[1] = datetime->tm_mon + 1;
    srm_time[2] = year >> 8;
    srm_time[3] = year & 0x00FF;
    srm_time[4] = datetime->tm_hour;
    srm_time[5] = datetime->tm_min;
    srm_time[6] = datetime->tm_sec;
    return srm_msg_exchange(handle, SRM_CMD_GET_DATETIME, srm_time, sizeof(srm_time), buff, sizeof(buff), &l);
}


size_t srm_msg_exchange_ex(srm_handle_t *handle, const unsigned char *cmd,
                        unsigned char *buffer, size_t buff_size, size_t *out_size)
{
    return srm_msg_exchange(handle, cmd, NULL, 0, buffer, buff_size, out_size);
}


