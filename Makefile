#CC = gcc
LDLIBS = -lcurses -lexpat

INSTALL = install -c
PREFIX = usr
BINDIR = bin
DATADIR = share

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

all: $(TARGET) $(TARGET).1

OBJS = display.o file.o gnucash.o list.o main.o tree.o xml.o

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $@

$(TARGET).1: manpage.1.in
	sed -e 's,@PROGNAME@,$(TARGET),g' < $< > $@

clean:
	rm -rf $(TARGET) $(TARGET).1 $(TARGET).tgz *.bb *.bbg *.da *.gcov *.o core gmon.out

dist:
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile README manpage.1.in *.[ch] tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

install: $(TARGET) $(TARGET).1
	-mkdir -p $(DESTDIR)/$(PREFIX)/$(BINDIR)
	-$(INSTALL) $(TARGET) $(DESTDIR)/$(PREFIX)/$(BINDIR)
	-mkdir -p $(DESTDIR)/$(PREFIX)/$(DATADIR)/man/man1
	-$(INSTALL) -m 644 $(TARGET).1 $(DESTDIR)/$(PREFIX)/$(DATADIR)/man/man1

test: $(TARGET)
	./$(TARGET) gcd

display.o: display.c main.h list.h
file.o: file.c main.h list.h
gnucash.o: gnucash.c main.h list.h tree.h xml.h
list.o: list.c list.h
main.o: main.c main.h list.h
tree.o: tree.c tree.h
xml.o: xml.c list.h xml.h
