CC = g++
FLAGS = -std=c++17 -pedantic -Wall

all: kompilator

kompilator: lex.yy.o parser.tab.o compiler.o
	$(CC) $(FLAGS) -o kompilator lex.yy.o parser.tab.o compiler.o

parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

lex.yy.c: lexer.l parser.tab.h
	flex lexer.l

lex.yy.o: lex.yy.c
	$(CC) $(FLAGS) -c lex.yy.c

parser.tab.o: parser.tab.c
	$(CC) $(FLAGS) -c parser.tab.c

compiler.o: compiler.cpp compiler.h
	$(CC) $(FLAGS) -c compiler.cpp

clean:
	rm -f *.o kompilator lex.yy.c parser.tab.c parser.tab.h
