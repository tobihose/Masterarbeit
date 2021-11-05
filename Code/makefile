CC=gcc
CFLAGS=-Wall -m64 -g -arch x86_64
LIBS=-lm -lgurobi91
INCLUDES=-I. -Iinclude
SRC=src/
VPATH=./src

SampleAlg: main.c wasser_wh.c algorithm.c
	$(CC) $(CFLAGS) $(LIBS) $(INCLUDES) src/main.c src/wasser_wh.c src/algorithm.c  -o SampleAlg
grayf: grayf.c grayscale_m.c
	$(CC) $(CFLAGS) $(LIBS) $(INCLUDES) src/grayf.c src/grayscale_m.c -o grayf 

