## VARIABLES

#CC = gcc
LDLIBS = -lcurses -lexpat

INSTALL = install -c
STRIP = strip
PREFIX = usr
BINDIR = bin
DATADIR = share
MANDIR = $(DESTDIR)/$(PREFIX)/$(DATADIR)/man/man1

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
			-Wundef
ifeq "$(DEBUG)" ""
NITPICKY_WARNINGS +=	-Wuninitialized
endif
NITPICKY_WARNINGS += \
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
LDFLAGS += -pg -O3
else
ifneq "$(DEBUG)" ""
CFLAGS += -g3 -DDEBUG -O0
else
CFLAGS += -O3
LDFLAGS += -O3 -s
endif
endif

TARGET = stark

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(patsubst %.c, %.o, $(SRCS))
DEPS = $(patsubst %.c, .%.d, $(SRCS))

export CC TARGET SRCS OBJS CFLAGS LDFLAGS LDLIBS

## TARGETS

all: $(TARGET) $(TARGET).1

$(TARGET): depend
	@$(MAKE) -f .depend $(TARGET)

$(TARGET).1: manpage.1.in
	sed -e 's,@PROGNAME@,$(TARGET),g' < $< > $@

tags: $(SRCS) $(HDRS)
	ctags $(SRCS) $(HDRS)

mostlyclean:
	rm -f $(OBJS)
	rm -f *.bb *.bbg *.da

clean: mostlyclean
	rm -f *.gcov
	rm -f $(TARGET) $(TARGET).1
	rm -f core gmon.out

distclean: clean
	rm -f $(DEPS) .depend
	rm -f tags
	rm -rf tmp
	rm -f $(TARGET).tgz

dist:
	rm -rf tmp
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile README manpage.1.in $(SRCS) $(HDRS) tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

install: $(TARGET) $(TARGET).1
	@-mkdir -p $(DESTDIR)/$(PREFIX)/$(BINDIR)
	$(INSTALL) $(TARGET) $(DESTDIR)/$(PREFIX)/$(BINDIR)
	@-mkdir -p $(MANDIR)
	$(INSTALL) -m 644 $(TARGET).1 $(MANDIR)

install-strip: install
	$(STRIP) $(DESTDIR)/$(PREFIX)/$(BINDIR)/$(TARGET)

test: $(TARGET)
	./$(TARGET) ~/financial/gcd

# rebuilding every .d every time any .h changes is probably overkill
.%.d: %.c $(HDRS)
	$(CC) -MM $(CFLAGS) $< > $@

.depend:
	echo "include $(DEPS)" > $@
	echo >> $@
	echo "\$$(TARGET): \$$(OBJS)" >> $@
	echo "	\$$(CC) \$$(LDFLAGS) \$$(OBJS) \$$(LDLIBS) -o \$$@" >> $@

depend: $(DEPS) .depend

.PHONY: depend mostlyclean clean dist distclean install install-strip test
