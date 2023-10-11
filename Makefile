LD=$(CC)
VPATH=src

CFLAGS = -D_XOPEN_SOURCE=600 -O2 -g -Wall -Wextra -std=c99 \
	 -Wcast-qual -Wshadow -Wconversion -Wsign-compare \
	 -Wformat -Wformat-security -Wmissing-prototypes -Wstrict-prototypes \
	 `pkg-config --cflags gtk+-3.0 libevdev`

LDFLAGS = `pkg-config --libs gtk+-3.0 libevdev`


PROG = evdev-js-test
OBJS = main.o app-window.o device-list-widget.o event-widget.o joystick.o \
       vice.o button-widget.o

$(PROG): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(PROG)

.PHONY: clean
clean:
	rm -f $(OBJS)
	rm -f $(PROG)
