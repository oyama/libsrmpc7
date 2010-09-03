CFLAGS=-I/usr/local/include
LIBS=-L/usr/local/lib -lftd2xx
DEBUG=

LIBSRMPC7_OBJ=srmpc7.o

.c.o:
	$(CC) -Wall $(CFLAGS) $(DEBUG) -c $<


all: libsrmpc7.a srmcat srmsync


libsrmpc7.a: srmpc7.h $(LIBSRMPC7_OBJ)
	$(AR) -csr $@ $(LIBSRMPC7_OBJ)

srmcat: libsrmpc7.a srmcat.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)

srmsync: libsrmpc7.a srmsync.o
	$(CC) $(CFLAGS) -o $@ $@.o libsrmpc7.a $(LIBS)


mock: mockup

mockup: libmockftd2xx.a mock_srmcat mock_srmsync

libmockftd2xx.a: libmockftd2xx.o _mock_data.h
	$(AR) -csr libmockftd2xx.a libmockftd2xx.o
mock_srmcat: libsrmpc7.a srmcat.o
	$(CC) $(CFLAGS) -o mock_srmcat srmcat.o libsrmpc7.a libmockftd2xx.a
mock_srmsync: libsrmpc7.a srmsync.o
	$(CC) $(CFLAGS) -o mock_srmsync srmsync.o libsrmpc7.a libmockftd2xx.a


clean:
	rm -f *.a *.o mock_* srmcat srmsync
