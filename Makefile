## VARIABLES

#CC = gcc
LDLIBS = -lcurses -lexpat

INSTALL = install -c
PREFIX = usr
BINDIR = bin
DATADIR = share

ifneq "$(PICKY)" ""
NITPICKY_WARNINGS = \
		    -W \
		    -Waggregate-return \
		    -Wall \
		    -Wbad-function-cast \
		    -Wcast-align \
		    -Wcast-qual \
		    -Wchar-subscripts \
		    -Wcomment \
		    -Wdisabled-optimization \
		    -Wendif-labels \
		    -Werror \
		    -Wfloat-equal \
		    -Wformat=2 \
		    -Wimplicit \
		    -Winline \
		    -Wmain \
		    -Wmissing-braces \
		    -Wmissing-declarations \
		    -Wmissing-format-attribute \
		    -Wmissing-noreturn \
		    -Wmissing-prototypes \
		    -Wnested-externs \
		    -Wnonnull \
		    -Wpacked \
		    -Wpadded \
		    -Wparentheses \
		    -Wpointer-arith \
		    -Wredundant-decls \
		    -Wreturn-type \
		    -Wsequence-point \
		    -Wshadow \
		    -Wsign-compare \
		    -Wstrict-aliasing \
		    -Wstrict-prototypes \
		    -Wswitch \
		    -Wswitch-enum \
		    -Wtrigraphs \
		    -Wundef \
		    -Wunknown-pragmas \
		    -Wunused \
		    -Wwrite-strings \
		    -pedantic \
		    -std=c99 \

ifeq "$(DEBUG)" ""
NITPICKY_WARNINGS += -Wuninitialized
endif
CFLAGS += $(NITPICKY_WARNINGS)
endif

# PERF and DEBUG are mutually exclusive, and PERF takes precendence
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
OBJS = $(patsubst %.c, %.o, $(SRCS))
DEPS = $(patsubst %.c, .%.d, $(SRCS))

export CC TARGET SRCS OBJS CFLAGS LDLIBS

## TARGETS

all: $(TARGET) $(TARGET).1

$(TARGET): $(DEPS) .depend
	@$(MAKE) -f .depend $(TARGET)

$(TARGET).1: manpage.1.in
	sed -e 's,@PROGNAME@,$(TARGET),g' < $< > $@

clean:
	rm -f *.o $(TARGET) $(TARGET).1
	rm -f *.bb *.bbg *.da *.gcov
	rm -f core gmon.out

distclean: clean
	rm -f $(DEPS) .depend
	rm -rf tmp
	rm -f $(TARGET).tgz

dist:
	rm -rf tmp
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile README manpage.1.in *.[ch] tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

install: $(TARGET) $(TARGET).1
	@-mkdir -p $(DESTDIR)/$(PREFIX)/$(BINDIR)
	$(INSTALL) $(TARGET) $(DESTDIR)/$(PREFIX)/$(BINDIR)
	@-mkdir -p $(DESTDIR)/$(PREFIX)/$(DATADIR)/man/man1
	$(INSTALL) -m 644 $(TARGET).1 $(DESTDIR)/$(PREFIX)/$(DATADIR)/man/man1

test: $(TARGET)
	./$(TARGET) ~/financial/gcd

# rebuilding every .d every time any .h changes is probably overkill
.%.d: %.c $(HDRS)
	$(CC) -MM $(CFLAGS) $< > $@

.depend:
	echo -e "include $(DEPS)" > $@
	echo -e "\n\$$(TARGET): \$$(OBJS)" >> $@
	echo -e "\t\$$(CC) \$$(OBJS) \$$(LDLIBS) -o \$$@" >> $@

depend: $(DEPS) .depend

.PHONY: depend clean dist distclean install test
