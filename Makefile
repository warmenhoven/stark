CC = gcc
LDLIBS = -lcurses -lexpat

ifneq "$(PICKY)" ""
NITPICKY_WARNINGS = -Werror \
		    -Wall \
		    -W \
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
		    -std=c99 \
		    -pedantic

CFLAGS += $(NITPICKY_WARNINGS)
endif

ifneq "$(PERF)" ""
CFLAGS += -pg -fprofile-arcs -ftest-coverage -O3
LDLIBS += -pg
else
ifneq "$(DEBUG)" ""
CFLAGS += -g3 -DDEBUG -O0
else
CFLAGS += -O3
endif
endif

TARGET = stark

OBJS = display.o file.o gnucash.o list.o main.o tree.o xml.o

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

# yeah, i know, this isn't strictly correct. so shoot me. they're small files.
$(OBJS): list.h main.h tree.h xml.h

clean:
	rm -rf $(TARGET) *.bb *.bbg *.da *.gcov *.o core gmon.out $(TARGET).tgz

dist:
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)
	cp Makefile README *.[ch] tmp/$(TARGET)
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)
	rm -rf tmp

test: $(TARGET)
	./stark gcd

timetest: $(TARGET)
	STARK_TIME=1 time ./stark gcd
