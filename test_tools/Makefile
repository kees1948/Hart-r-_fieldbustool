
OBJECTS=hartcmdline.o
TARGET=.

CCFLAGS= -O0 -Wall	-Wunused -Wstrict-prototypes


hartcmdline:	$(OBJECTS)
		cc -g $(CCFLAGS) $(OBJECTS) -o hartcmdline 

hartcmdline.o:	hartcmdline.c hartcmdline.h Hart.h
		$(CC) -g $(CCFLAGS) -c hartcmdline.c

harttestframe:	Harttestframe.c
		$(CC) -g $(CCFLAGS) Harttestframe.c -o harttestframe

install:
		cp hartcmdline $(TARGET)
		
clean:
		rm *.o
