#include <stdio.h>
#include <string.h>
#include <ftd2xx.h>

#define CALL_FT_CLOSE -1
#define CALL_FT_WRITE  0
#define CALL_FT_READ   1


static int mock_io_count = 0;

typedef struct {
    int type;
    size_t length;
    unsigned char packet[264];
} mock_d2xx_t;

static mock_d2xx_t MOCK_IO[] = {
#include "_mock_data.h"
};


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


FT_STATUS FT_ListDevices(PVOID pArg1, PVOID pArg2, DWORD Flags)
{
    return FT_OK;
}


FT_STATUS FT_Open(int deviceNumber, FT_HANDLE *pHandle)
{
    mock_io_count = 0;
    return FT_OK;
}

FT_STATUS FT_OpenEx(PVOID pArg1, DWORD Flags, FT_HANDLE *pHandle)
{
    mock_io_count = 0;
    return FT_OK;
}


FT_STATUS FT_Write(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesWritten)
{
    mock_d2xx_t *io;
    io = &MOCK_IO[mock_io_count];
    mock_io_count++;
    if (io->type != CALL_FT_WRITE) {
        fprintf(stderr, "MOCK: is not CALL_FT_WRITE msg: %d, type=%d\n", mock_io_count, io->type);
        _print_hex("MOCK: CALL_FT_WRITE", io->packet, io->length);
        return FT_IO_ERROR;
    }

    *lpBytesWritten = io->length;
    return FT_OK;
}

FT_STATUS FT_Read(FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesReturned)
{
    mock_d2xx_t *io;
    io = &MOCK_IO[mock_io_count];
    mock_io_count++;
    if (io->type != CALL_FT_READ) {
        fprintf(stderr, "MOCK: is not CALL_FT_READ msg: %d, type=%d\n", mock_io_count, io->type);
        _print_hex("MOCK: CALL_FT_READ", io->packet, io->length);
        return FT_IO_ERROR;
    }

    if (nBufferSize < io->length) {
        fprintf(stderr, "MOCK: CALL_FT_READ msg: %d, type=%d, read buffer is short(%ld bytes)\n", mock_io_count, io->type, nBufferSize);
        return FT_IO_ERROR;
    }
    memmove(lpBuffer, io->packet, io->length);
    *lpBytesReturned = io->length;
    return FT_OK;
}

FT_STATUS FT_Close(FT_HANDLE ftHandle)
{
    return FT_OK;
}


FT_STATUS FT_ResetDevice(FT_HANDLE ftHandle)
{
    return FT_OK;
}


FT_STATUS FT_Purge(FT_HANDLE ftHandle, ULONG Mask)
{
    return FT_OK;
}


FT_STATUS FT_SetTimeouts(FT_HANDLE ftHandle, ULONG ReadTimeout, ULONG WriteTimeout)
{
    return FT_OK;
}


FT_STATUS FT_GetQueueStatus(FT_HANDLE ftHandle, DWORD *dwRxBytes)
{
    return FT_OK;
}


FT_STATUS FT_SetBaudRate(FT_HANDLE ftHandle, ULONG BaudRate)
{
    return FT_OK;
}


FT_STATUS FT_SetDataCharacteristics(FT_HANDLE ftHandle, UCHAR WordLength, UCHAR StopBits, UCHAR Parity)
{
    return FT_OK;
}


FT_STATUS FT_SetLatencyTimer(FT_HANDLE ftHandle, UCHAR ucTimer)
{
    fprintf(stderr, "MOCK: FT_SetLatencyTimer %u\n",ucTimer);
    return FT_OK;
}

