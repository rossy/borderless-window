CC = gcc
WINDRES = windres
STD = -std=c99
DEFS = -D_WIN32_WINNT=0x0602
CFLAGS = -g -Wall -municode
LDFLAGS = -municode -mwindows -Wl,--major-subsystem-version=6,--minor-subsystem-version=0
LIBS = -ldwmapi -luxtheme

EXE = borderless-window.exe
OBJ = borderless-window.o resources.o

all: $(EXE)
.PHONY: all

.SUFFIXES:
.SUFFIXES: .c .o .rc

resources.o: borderless-window.exe.manifest

.rc.o:
	$(WINDRES) $(CPPFLAGS) $(DEFS) --input=$< --output=$@

.c.o:
	$(CC) -c $(STD) $(CPPFLAGS) $(DEFS) $(CFLAGS) $< -o $@

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

clean:
	rm -f $(EXE) $(OBJ)
.PHONY: clean
