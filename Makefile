## VARIABLES

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

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = display.o file.o gnucash.o list.o main.o tree.o xml.o

export CC TARGET SRCS OBJS CFLAGS LDLIBS

## TARGETS

all: $(TARGET) $(TARGET).1

$(TARGET): .depend
	@$(MAKE) -f .depend $(TARGET)

$(TARGET).1: manpage.1.in
	sed -e 's,@PROGNAME@,$(TARGET),g' < $< > $@

clean:
	rm -rf .depend $(TARGET) $(TARGET).1 $(TARGET).tgz *.bb *.bbg *.da *.gcov *.o core gmon.out

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

# rebuilding every .depend every time any .c or .h changes is probably bad, we could fix that
.depend: $(SRCS) $(HDRS)
	@gcc -MM $(SRCS) > $@
	@echo -e "\n\$$(TARGET): \$$(OBJS)\n\t\$$(CC) \$$(OBJS) \$$(LDLIBS) -o \$$@" >> $@

.PHONY: depend

depend: .depend
