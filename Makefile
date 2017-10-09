

CXX=gcc 
LD=gcc

CFLAGS=-O2 -Wall


all:
	${CXX} ${CFLAGS} main.c -o ddflash
	
