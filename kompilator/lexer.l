%{
#include <string>
#include <iostream>
#include "parser.tab.h"

using namespace std;
%}

%option noyywrap
%option yylineno
%option nounput

%%

\#.*                  ;
[ \t\r\n]+            ;

"PROGRAM"             { return PROGRAM; }
"IS"                  { return IS; }
"IN"                  { return IN; }
"PROCEDURE"           { return PROCEDURE; }

"ENDWHILE"            { return ENDWHILE; }
"ENDIF"               { return ENDIF; }
"ENDFOR"              { return ENDFOR; }
"END"                 { return END; }
"IF"                  { return IF; }
"THEN"                { return THEN; }
"ELSE"                { return ELSE; }
"WHILE"               { return WHILE; }
"DO"                  { return DO; }
"REPEAT"              { return REPEAT; }
"UNTIL"               { return UNTIL; }
"FOR"                 { return FOR; }
"FROM"                { return FROM; }
"DOWNTO"              { return DOWNTO; }
"TO"                  { return TO; }

"READ"                { return READ; }
"WRITE"               { return WRITE; }

":="                  { return ASSIGN; }
"+"                   { return PLUS; }
"-"                   { return MINUS; }
"*"                   { return MULT; }
"/"                   { return DIV; }
"%"                   { return MOD; }
"="                   { return EQ; }
"!="                  { return NEQ; }
">="                  { return GEQ; }
"<="                  { return LEQ; }
">"                   { return GT; }
"<"                   { return LT; }

"T"                   { return KW_T; }
"I"                   { return KW_I; }
"O"                   { return KW_O; }

"["                   { return LBRACKET; }
"]"                   { return RBRACKET; }
"("                   { return LPAREN; }
")"                   { return RPAREN; }
":"                   { return COLON; }
";"                   { return SEMICOLON; }
","                   { return COMMA; }

[0-9]+                { yylval.num = stoll(yytext); return NUM; }
[_a-z]+               { yylval.str = new string(yytext); return PIDENTIFIER; }

.                     { cerr << "Błąd leksykalny w linii " << yylineno << ": " << yytext << endl; exit(1); }
%%