

CXX=gcc
LD=gcc

CFLAGS=-O2 -Wall -g


all:
	${CXX} ${CFLAGS} main.c -o ddflash
	
