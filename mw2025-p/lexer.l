/*
 * Lexer interpretera maszyny wirtualnej do projektu z JFTT2025
 *
 * Autor: Maciek Gębala
 * http://ki.pwr.edu.pl/gebala/
 * 2025-11-15
*/
%option noyywrap
%option yylineno
%option nounput
%{
#include "parser.hh"
#include "instructions.hh"

int yylex();
%}
%%
\#.*\n		;
[ \t]+          ;
READ            { yylval = READ;   return COM_0; };
WRITE           { yylval = WRITE;  return COM_0; };
LOAD            { yylval = LOAD;   return COM_1; };
STORE           { yylval = STORE;  return COM_1; };
RLOAD           { yylval = RLOAD;  return COM_2; };
RSTORE          { yylval = RSTORE; return COM_2; };
ADD             { yylval = ADD;    return COM_2; };
SUB             { yylval = SUB;    return COM_2; };
SWP             { yylval = SWP;    return COM_2; };
RST             { yylval = RST;    return COM_2; };
INC             { yylval = INC;    return COM_2; };
DEC             { yylval = DEC;    return COM_2; };
SHL             { yylval = SHL;    return COM_2; };
SHR             { yylval = SHR;    return COM_2; };
JUMP            { yylval = JUMP;   return JUMP_1; };
JPOS            { yylval = JPOS;   return JUMP_1; };
JZERO           { yylval = JZERO;  return JUMP_1; };
CALL		{ yylval = CALL;   return JUMP_1; };
RTRN		{ yylval = RTRN;   return JUMP_0; };
HALT            { yylval = HALT;   return STOP; };
a               { yylval = 0;      return REG; };
b               { yylval = 1;      return REG; };
c               { yylval = 2;      return REG; };
d               { yylval = 3;      return REG; };
e               { yylval = 4;      return REG; };
f               { yylval = 5;      return REG; };
g               { yylval = 6;      return REG; };
h               { yylval = 7;      return REG; };
[0-9]+		{ yylval = atoll( yytext );  return NUMBER; };
\n              ;
.               return ERROR;
%%
