#CC = gcc
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

all: $(TARGET)

OBJS = display.o file.o gnucash.o list.o main.o tree.o xml.o

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

$(TARGET).1: manpage.1.in
	sed -e 's,@PROGNAME@,$(TARGET),g' < $< > $@

clean:
	rm -rf $(TARGET) *.bb *.bbg *.da *.gcov *.o core gmon.out $(TARGET).tgz

dist: $(TARGET).1
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile README $(TARGET).1 *.[ch] tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

test: $(TARGET)
	./stark gcd

timetest: $(TARGET)
	STARK_TIME=1 time ./stark gcd

display.o: display.c main.h list.h
file.o: file.c main.h list.h
gnucash.o: gnucash.c main.h list.h tree.h xml.h
list.o: list.c list.h
main.o: main.c main.h list.h
tree.o: tree.c tree.h
xml.o: xml.c list.h xml.h
