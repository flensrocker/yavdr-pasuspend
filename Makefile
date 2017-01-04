
CXX = gcc
CFLAGS = $(shell pkg-config --cflags libpulse)
LDFLAGS =
LIBS = $(shell pkg-config --libs libpulse)

all: yavdr-pasuspend

%.o: %.c
	$(CXX) $(CFLAGS) -c -o $@ $<

yavdr-pasuspend: yavdr-pasuspend.o
	$(CXX) $(CFLAGS) $(LDFLAGS) yavdr-pasuspend.o $(LIBS) -o $@

clean:
	@-rm -f *.o yavdr-pasuspend

