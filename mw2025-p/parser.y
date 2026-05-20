/*
 * Parser interpretera maszyny wirtualnej do projektu z JFTT2025
 *
 * Autor: Maciek Gębala
 * http://ki.pwr.edu.pl/gebala/
 * 2025-11-15
*/
%code requires { 
#include<vector> 
#include<utility>

#define YYSTYPE long long

using namespace std;
}
%{
#include <iostream>
#include <utility>
#include <vector>

#include "instructions.hh"
#include "colors.hh"

using namespace std;

extern int yylineno;
int yylex( void );
void yyset_in( FILE * in_str );
void yyerror( vector< pair<int,long long> > & program, char const *s );

%}
%parse-param { vector< pair<int,long long> > & program }
%token COM_0
%token COM_1
%token COM_2
%token JUMP_0
%token JUMP_1
%token STOP
%token REG
%token NUMBER
%token ERROR
%%
input :
    input line
  | %empty
  ;

line :
    COM_0	  { program.push_back( make_pair($1,0) ); }
  | COM_1 NUMBER  { program.push_back( make_pair($1,$2) ); }
  | COM_2 REG	  { program.push_back( make_pair($1,$2) ); }
  | JUMP_0        { program.push_back( make_pair($1,0) ); }
  | JUMP_1 NUMBER { program.push_back( make_pair($1,$2) ); }
  | STOP          { program.push_back( make_pair($1,0) ); }
  | ERROR         { yyerror( program, "Nierozpoznany symbol" ); }
  ;
%%
void yyerror( vector< pair<int,long long> > & program, char const *s )
{
  cerr << cRed << "Linia " << yylineno << ": " << s << cReset << endl;
  exit(-1);
}

void run_parser( vector< pair<int,long long> > & program, FILE * data ) 
{
  cout << cBlue << "Czytanie kodu." << cReset << endl;
  yyset_in( data );
  yyparse( program );
  cout << cBlue << "Skończono czytanie kodu (liczba rozkazów: " << program.size() << ")." << cReset << endl;
}
