TARGET = png2pet

CC            = $(PREFIX)gcc
CXX           = $(PREFIX)g++
LB            = $(PREFIX)ar rsv
AS            = $(PREFIX)as
LD            = $(PREFIX)gcc
NM            = $(PREFIX)nm
OBJCOPY       = $(PREFIX)objcopy
GCC           = $(CC)


OBJECTS += objs/png2pet.o
OBJECTS += objs/gifenc.o
OBJECTS += objs/gifdecmem.o
OBJECTS += objs/pucrunch.o
OBJECTS += objs/decrunch.o

CFLAGS  += -O2 -Wall -DHAVE_CONFIG_H -Wno-system-headers
INCLUDES += -I.
LDADD += -lpng


all: $(TARGET)

png2pet: $(OBJECTS)
	$(CXX) $(LDFLAGS) $+ $(LDADD) -o $(TARGET)
	
objs/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $< $(INCLUDES) $(CPPFLAGS)

clean:
	rm -f $(OBJECTS) $(TARGET)

