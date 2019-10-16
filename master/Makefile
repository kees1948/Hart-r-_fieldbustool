#
#
# target master_full is version with database and tablefile support
#
# target master_tf is version with tablefile suport only
#
#

OBJECTSF=DataLink.o Main.o ProcessFrames.o
OBJECTST=DataLink.o Maint.o ProcessFramest.o
TARGET=.

CCFLAGS= -O2 -Wall	-Wunused -Wstrict-prototypes
#
# for full-fledged version define all
#
#BUILD_DEFINES=-DHAS_TABLEFILE -DHAS_DATABASE
BUILD_DEFINES=-DHAS_TABLEFILE

#
# build full fledged version
#
master_full:	$(OBJECTSF)
		cc -g $(CCFLAGS) $(OBJECTSF) -o master -lpthread -lmysqlclient -lncurses 
#-ltinfo

#
# build table file version only
#
master_tf:	$(OBJECTST)
		cc -g $(CCFLAGS) $(OBJECTST)  -o master -lpthread -lmysqlclient -lncurses 
#-ltinfo



Main.o:	Main.c Main.h Hart.h
		$(CC) -g $(CCFLAGS) $(BUILD_DEFINES) -DHAS_TABLEFILE -DHAS_DATABASE -c Main.c -o $@

Maint.o:	Main.c Main.h Hart.h
		$(CC) -g $(CCFLAGS) $(BUILD_DEFINES) -DHAS_TABLEFILE -c Main.c -o $@

DataLink.o:	DataLink.c	DataLink.h Hart.h
		$(CC) -g $(CCFLAGS) $(BUILD_DEFINES) -c DataLink.c -o $@

ProcessFrames.o:	ProcessFrames.c Hart.h
		$(CC) -g $(CCFLAGS) $(BUILD_DEFINES) -DHAS_TABLEFILE -DHAS_DATABASE -c ProcessFrames.c -o $@

ProcessFramest.o:	ProcessFrames.c Hart.h
		$(CC) -g $(CCFLAGS) $(BUILD_DEFINES) -DHAS_TABLEFILE -c ProcessFrames.c -o $@

install:
		cp master $(TARGET)
		
clean:
		rm *.o
