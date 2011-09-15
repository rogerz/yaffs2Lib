CC 				= ccarm
AR				= ararm
RM				= rm
INCS 			= -I. -I$(BSP_DIR) -I$(WIND_BASE)/target/h
DEFS			= -DCPU=XSCALE -DTOOL_FAMILY=gnu -DTOOL=gnube
CFLAGS			= -g -mapcs-32 -mbig-endian -mcpu=xscale -fno-builtin -DARMEB $(INCS) $(DEFS)
CFLAGS_AS		= -g -mapcs-32 -mbig-endian -mcpu=xscale -ansi -fno-builtin -P -xassembler-with-cpp $(INCS) $(DEFS)
ARFLAGS			= crus

# Local defines
BLDLOCAL	=	build
BLDDIR		=	$(PRJ_BASE_NO_DRIVE)/$(BLDLOCAL)
LIBDIR		=	$(PRJ_BASE)/lib

DIRS		=	nandflash yaffs2 yaffs2/yaffs2Lib
INCS		+= $(foreach DIR, $(DIRS), $(addprefix -I,$(DIR)))

SRCS	=	$(foreach DIR, $(DIRS), $(wildcard $(DIR)/*.c))
HDRS	=	$(foreach DIR, $(DIRS), $(wildcard $(DIR)/*.h))
OBJS	= 	$(addprefix $(BLDDIR)/, $(SRCS:.c=.o))
DEPS	=	$(addprefix $(BLDDIR)/, $(SRCS:.c=.d))

TREE	=	$(PRJ_BASE_NO_DRIVE)/$(BLDLOCAL)/.tree

.PHONY:	all clean veryclean depend dirs

.PRECIOUS: $(OBJS) $(DEPS)

lib = flashDisk

TARGET	=	$(LIBDIR)/$(lib).a
Q = @

DIRHOLDERS = $(foreach DIR, $(DIRS), $(BLDDIR)/$(DIR)/.holder)

$(TARGET):	$(OBJS)
	$Qecho Building library $@
	$Q$(AR) $(ARFLAGS) $@ $?

$(OBJS): $(BLDDIR)/%.o: %.c
	$Qecho Compiling $<
	$Q$(CC) $(CFLAGS) -c $< -o $@

$(DEPS): $(BLDDIR)/%.d: %.c $(TREE)
	$Qecho Generating dependency for $<
	$Q$(CC) -M $(CFLAGS) $< > $(BLDDIR)/$*.dd
	$Qsed -e "1s/^\(.*\)$$/$(subst /,\/,$(subst \,\\\\,$(dir $@)))\1/" $(BLDDIR)/$*.dd > $(BLDDIR)/$*.d
	$Qrm $(BLDDIR)/$*.dd

$(TREE): $(DIRHOLDERS)
	$Qecho done > $@

clean:
	$Qecho Removing target and objs
	$Q$(RM) $(TARGET) $(OBJS)

veryclean:
	$Qecho Cleaning up
	$Q$(RM) $(TARGET) $(OBJS) $(DEPS)

depend: $(DEPS)

$(DIRHOLDERS): %/.holder :
	$Qecho Making build dir $*
	$Qmkdir $(subst /,\,$*)
	$Qecho placeholder for $* > $@

all:	$(TARGET)

-include $(DEPS)
