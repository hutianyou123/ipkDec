# Makefile

ipkDec : ipkDec.o getopt.o
	g++ -o ipkDec ipkDec.o getopt.o -lm 
	
ipkDec.o : ipkDec.cpp
	g++ -c ipkDec.cpp
getopt.o : getopt.c
	gcc -c  getopt.c

clean:	
	rm  ipkDec ipkDec.o getopt.o
	