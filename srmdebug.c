#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "srmpc7.h"

static void _print_hex(const char *label, unsigned char *src, int len)
{
    const char *hex = "0123456789abcdef";
    int i;
    char buff[3];
    fprintf(stderr, "%s", label);
    for (i = 0; i < len; i++) {
        buff[0] = hex[src[i] >> 4];
        buff[1] = hex[src[i]&0x0f];
        buff[2] = '\0';
        fprintf(stderr, "%02x ", src[i]);
        if (i != 0 && i % 16 == 0)
            fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
}




static srm_handle_t *current_device = NULL;

static void cleanup()
{
    fprintf(stderr, "catch signal!\n");
    if (current_device == NULL)
        exit(1);
    srm_close(current_device);
    exit(1);
}


char hex_to_char(const char str[ ])
{
    short i = 0;
    short n;
    unsigned char x = 0;
    char c;

    while (str[i] != '\0') {
        if ('0' <= str[i] && str[i] <= '9')
            n = str[i] - '0';
        else if ('a' <= (c = tolower(str[i])) && c <= 'f')
            n = c - 'a' + 10;
        else {
            exit(-1);
        }
        i++;
        x = x *16 + n;
    }
    return x;
}


int main(int argc, char *argv[])
{
    srm_handle_t *srm;
    srm_ride_block_t *file;
    srm_ride_record_t record;
    int rc, num;
    char cmd[2];
    unsigned char buff[SRM_PACKET_SIZE];
    size_t l, out_l;

    if ((srm = srm_open(SRM_DEVICE_NAME_PC7)) == NULL) {
        fprintf(stderr, "can't open device \"%s\": %s\n", SRM_DEVICE_NAME_PC7, srm_get_error_message());
        return 1;
    }
    current_device = srm;
    signal(SIGINT, cleanup);
    signal(SIGTRAP, cleanup);

    cmd[0] = hex_to_char(argv[1]);
    cmd[1] = hex_to_char(argv[2]);
    _print_hex("COMMAND = ", (unsigned char *)cmd, sizeof(cmd));
    rc = srm_msg_exchange_ex(srm, cmd, buff, sizeof(buff), &l);
    _print_hex("RESPONSE= ", buff, l);

    srm_close(srm);
    current_device = NULL;
    return 0;
}
