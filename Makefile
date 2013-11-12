CFLAGS=-I/usr/local/include
LIBS=-L/usr/local/lib -lftd2xx
DEBUG=

LIBSRMPC7_OBJ=srmpc7.o
SRMSYNC_OBJ=srmsync.o file_tcx.o


.c.o:
	$(CC) -Wall $(CFLAGS) $(DEBUG) -c $<


all: libsrmpc7.a srmcat srmsync srmonline srmerase srmbattery srmdatetime


libsrmpc7.a: srmpc7.h $(LIBSRMPC7_OBJ)
	$(AR) -csr $@ $(LIBSRMPC7_OBJ)

srmcat: libsrmpc7.a srmcat.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)

srmsync: libsrmpc7.a $(SRMSYNC_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SRMSYNC_OBJ) libsrmpc7.a $(LIBS)

srmonline: libsrmpc7.a srmonline.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)

srmerase: libsrmpc7.a srmerase.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)

srmvt: libsrmpc7.a srmvt.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)

srmdebug: libsrmpc7.a srmdebug.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)

srmbattery: libsrmpc7.a srmbattery.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)

srmdatetime: libsrmpc7.a srmdatetime.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)


mock: mockup

mockup: libmockftd2xx.a mock_srmcat mock_srmsync

libmockftd2xx.a: libmockftd2xx.o _mock_data.h
	$(AR) -csr libmockftd2xx.a libmockftd2xx.o
mock_srmcat: libsrmpc7.a srmcat.o
	$(CC) $(CFLAGS) -o mock_srmcat srmcat.o libsrmpc7.a libmockftd2xx.a
mock_srmsync: libsrmpc7.a $(SRMSYNC_OBJ)
	$(CC) $(CFLAGS) -o mock_srmsync $(SRMSYNC_OBJ) libsrmpc7.a libmockftd2xx.a


clean:
	rm -f *.a *.o mock_* srmcat srmsync srmonline srmerase srmvt srmdebug srmbattery srmdatetime
