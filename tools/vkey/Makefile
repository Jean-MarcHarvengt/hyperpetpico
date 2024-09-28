TARGET = vkey

CC            = $(PREFIX)gcc
CXX           = $(PREFIX)g++
LB            = $(PREFIX)ar rsv
AS            = $(PREFIX)as
LD            = $(PREFIX)gcc
NM            = $(PREFIX)nm
OBJCOPY       = $(PREFIX)objcopy
GCC           = $(CC)


OBJECTS += objs/vkey.o



CFLAGS  += #-O2 -Wall -DHAVE_CONFIG_H -Wno-system-headers
INCLUDES += -I. -I/usr/local/Cellar/libusb/1.0.27/include/libusb-1.0
LDADD += -L/usr/local/Cellar/libusb/1.0.27/lib/ -lusb-1.0 -lncurses
CPPFLAGS += #-std=c++11

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $+ $(LDADD) -o $(TARGET)
	
objs/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $< $(INCLUDES)

objs/%.o: %.cpp
	$(CXX) $(CPPFLAGS) -o $@ -c $< $(INCLUDES)

clean:
	rm -f $(OBJECTS) $(TARGET)

