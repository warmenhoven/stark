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
		    -Wuninitialized \
		    -Wunknown-pragmas \
		    -Wunused \
		    -Wwrite-strings \
		    -pedantic \
		    -std=c99 \

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

export CC TARGET SRCS OBJS CFLAGS LDLIBS

## TARGETS

all: $(TARGET) $(TARGET).1

$(TARGET): .depend
	@$(MAKE) -f .depend $(TARGET)

$(TARGET).1: manpage.1.in
	sed -e 's,@PROGNAME@,$(TARGET),g' < $< > $@

clean:
	rm -rf .depend \
		*.o $(TARGET) $(TARGET).1 \
		$(TARGET).tgz \
		*.bb *.bbg *.da *.gcov \
		core gmon.out

dist:
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
	./$(TARGET) gcd

# rebuilding every .depend every time any .c or .h changes is probably bad
.depend: $(SRCS) $(HDRS)
	$(CC) -MM $(CFLAGS) $(SRCS) > $@
	@echo -e "\n\$$(TARGET): \$$(OBJS)" >> $@
	@echo -e "\t\$$(CC) \$$(OBJS) \$$(LDLIBS) -o \$$@" >> $@

depend: .depend

.PHONY: depend clean dist install
