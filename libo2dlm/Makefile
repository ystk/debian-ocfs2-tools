TOPDIR = ..

include $(TOPDIR)/Preamble.make

INCLUDES = -I. -I$(TOPDIR)/include

LIBRARIES = libo2dlm.a

CFLAGS += -fPIC

ifneq ($(BUILD_FSDLM_SUPPORT),)
DEFINES += -DHAVE_FSDLM
endif

ifneq ($(OCFS2_DEBUG_EXE),)
DEBUG_EXE_FILES = $(shell awk '/DEBUG_EXE/{if (k[FILENAME] == 0) {print FILENAME; k[FILENAME] = 1;}}' $(CFILES))
DEBUG_EXE_PROGRAMS = $(addprefix debug_,$(subst .c,,$(DEBUG_EXE_FILES)))

.SECONDARY:

UNINST_PROGRAMS += $(DEBUG_EXE_PROGRAMS)

debug_%.o : %.c 
	$(CC) $(CFLAGS) $(LOCAL_CFLAGS) $(CPPFLAGS) $(LOCAL_CPPFLAGS) \
		$(INCLUDES) $(DEFINES) \
		-DDEBUG_EXE -o $@ -c $<

debug_%: debug_%.o libo2dlm.a
	$(LINK) $(COM_ERR_LIBS)

endif

CFILES = o2dlm.c
HFILES = libdlm-compat.h

TESTING_CFILES = o2dlm_test.c

HFILES_GEN = o2dlm_err.h

ifeq ($(LIBDLM_FOUND),)
HFILES_GEN += libdlm.h
CLEAN_RULES += clean-libdlm

libdlm.h: libdlm-compat.h
	ln -s $< $@

clean-libdlm:
	rm -f libdlm.h

endif

OBJS = $(subst .c,.o,$(CFILES)) \
	o2dlm_err.o

$(OBJS): $(HFILES) $(HFILES_GEN)

TESTING_OBJS = $(subst .c,.o,$(TESTING_CFILES))

o2dlm_err.c o2dlm_err.h: o2dlm_err.et
	compile_et o2dlm_err.et

libo2dlm.a: $(OBJS)
	rm -f $@
	$(AR) r $@ $^
	$(RANLIB) $@

o2dlm_test: $(TESTING_OBJS) $(LIBRARIES) $(COM_ERR_LIBS)
	$(LINK) $(DL_LIBS)

DIST_FILES = $(CFILES) $(HFILES) o2dlm_err.et $(TESTING_CFILES)

CLEAN_RULES += clean-err clean-test

clean-err:
	rm -f o2dlm_err.c o2dlm_err.h

clean-test:
	rm -f o2dlm_test

include $(TOPDIR)/Postamble.make
