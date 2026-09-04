#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

MPEGSRCDIR := $(APPSDIR)/plugins/mpegplayer
MPEGBUILDDIR := $(BUILDDIR)/apps/plugins/mpegplayer

ROCKS += $(MPEGBUILDDIR)/mpegplayer.rock

MPEG_SRC := $(call preprocess, $(MPEGSRCDIR)/SOURCES)
MPEG_OBJ := $(call c2obj, $(MPEG_SRC))

# add source files to OTHER_SRC to get automatic dependencies
OTHER_SRC += $(MPEG_SRC)

# Set '-fgnu89-inline' if supported (GCCVER >= 4.1.3, GCCNUM > 401)
ifeq ($(shell expr $(GCCNUM) \> 401),1)
    MPEGCFLAGS = $(PLUGINFLAGS) -fgnu89-inline
else
    MPEGCFLAGS = $(PLUGINFLAGS)
endif

# R2-F4/DD-11 (M-059): metro_palette.h is a pure #define header (no
# metro_palette.c, nothing to link) -- safe to include directly from a
# plugin. Zero new RGB literals in this plugin; every color still comes
# from that single file (CLAUDE.md's palette rule).
MPEGCFLAGS += -I$(APPSDIR)/metro
# Metro (M-101): the dependency pass (mkdepfile, tools/functions.make)
# builds its command line from PPCFLAGS + OTHER_INC, NOT from
# MPEGCFLAGS -- so without this line `metro_palette.h` is unresolvable
# there and `-MG` turns it into a phony $(BUILDDIR)/metro_palette.h
# that no rule can make. It only bites on a build directory whose
# make.dep is generated AFTER M-059, which is why it stayed hidden:
# the working tree's build-ipod6g/ still carries a make.dep from
# before that phase. Every other plugin include path is registered the
# same way (apps/plugins/plugins.make:79).
OTHER_INC += -I$(APPSDIR)/metro

$(MPEGBUILDDIR)/mpegplayer.rock: $(MPEG_OBJ) $(CODECDIR)/libmad-mpeg.a

$(MPEGBUILDDIR)/%.o: $(MPEGSRCDIR)/%.c $(MPEGSRCDIR)/mpegplayer.make
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) -I$(dir $<) $(MPEGCFLAGS) -c $< -o $@
