CC = gcc
LDLIBS = -lcurses -lexpat

ifneq "$(DEBUG)" ""
CFLAGS += -g -DDEBUG
endif

ifneq "$(PICKY)" ""
CFLAGS += -Wall -Werror
NITPICKY_WARNINGS = -W \
		    -Wundef \
		    -Wendif-labels \
		    -Wshadow \
		    -Wpointer-arith \
		    -Wcast-qual \
		    -Wcast-align \
		    -Wwrite-strings \
		    -Wsign-compare \
		    -Waggregate-return \
		    -Wstrict-prototypes \
		    -Wmissing-prototypes \
		    -Wmissing-declarations \
		    -Wpadded \
		    -Wredundant-decls \
		    -Wnested-externs \
		    -Winline \
		    -ansi \
		    -pedantic

CFLAGS += $(NITPICKY_WARNINGS)
endif

OBJS = display.o gnucash.o list.o main.o xml.o

ifneq "$(PERF)" ""
CFLAGS += -fprofile-arcs -ftest-coverage -O0
else
CFLAGS += -O3
endif

TARGET = stark

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

# yeah, i know, this isn't strictly correct. so shoot me. they're small files.
$(OBJS): list.h main.h xml.h

clean:
	rm -rf $(TARGET) *.o core $(TARGET).tgz

dist:
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)
	cp Makefile README *.[ch] tmp/$(TARGET)
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)
	rm -rf tmp

test: $(TARGET)
	./stark gcd
