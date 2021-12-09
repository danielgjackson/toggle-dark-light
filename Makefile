# To build with native Windows toolchain use: build.cmd
#
# To cross-compile from WSL:
#
# wsl sudo apt install build-essential gcc-mingw-w64 && wsl make

BIN_NAME = toggle.exe
CC = x86_64-w64-mingw32-gcc
CFLAGS = -m64 -O3 -Wall -municode -DUNICODE -D_UNICODE
LIBS = -luser32 -lgdi32 -lcomctl32 -lshell32 -ladvapi32 -lcomdlg32 -lole32 -loleaut32 -lwbemuuid -ldxva2 -lversion

RES = $(wildcard *.rc)
SRC = $(wildcard *.c)
INC = $(wildcard *.h)

all: $(BIN_NAME)

$(BIN_NAME): Makefile $(SRC) $(INC) $(RES)
	x86_64-w64-mingw32-windres -i $(RES) -o $(RES:.rc=_res.o)
	$(CC) -std=c99 -o $(BIN_NAME) $(CFLAGS) $(SRC) $(RES:.rc=_res.o) -I/usr/x86_64-w64-mingw32/include -I/usr/local/include -L/usr/x86_64-w64-mingw32/lib -L/usr/local/lib $(LIBS)

clean:
	rm -f *.o $(BIN_NAME)
