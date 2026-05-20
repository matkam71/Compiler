%{
#include <iostream>
#include <string>
#include "compiler.h"

using namespace std;

extern int yylex();
extern int yylineno;
extern FILE *yyin;
void yyerror(const char *s);

Compiler compiler;
%}
%union {
    long long num;
    std::string* str;
}

%token <str> PIDENTIFIER
%token <num> NUM
%token PROGRAM IS IN END
%token PROCEDURE
%token KW_T KW_I KW_O
%token IF THEN ELSE ENDIF
%token WHILE DO ENDWHILE
%token REPEAT UNTIL
%token FOR FROM TO DOWNTO ENDFOR
%token READ WRITE
%token ASSIGN
%token PLUS MINUS MULT DIV MOD
%token EQ NEQ GT LT GEQ LEQ
%token LBRACKET RBRACKET LPAREN RPAREN COLON SEMICOLON COMMA

%type <str> identifier
%type <str> value
%type <str> proc_head
%type <str> type

%nonassoc IFX
%nonassoc ELSE

%%

program_all:
    procedures main
;

procedures:
    procedures procedure
    | 
;

procedure:
    PROCEDURE proc_head IS
        { 
          compiler.proc_start(*$2);   
          delete $2;
        }
    declarations IN commands END
        {
          compiler.proc_end();
        }
  | PROCEDURE proc_head IS
        {
          compiler.proc_start(*$2);
          delete $2;
        }
    IN commands END
        {
          compiler.proc_end();
        }
;

proc_head:
    PIDENTIFIER LPAREN args_decl RPAREN { $$ = $1; }
;

main:
    PROGRAM IS { compiler.program_start(); } declarations IN commands END { compiler.program_end(); }
    | PROGRAM IS { compiler.program_start(); } IN commands END { compiler.program_end(); }
;

commands:
    commands command
    | command
;

command:
    PIDENTIFIER ASSIGN expression SEMICOLON 
    {
        string* id = $1; 
        compiler.assign_var(*id);
        delete id;
    }
    | PIDENTIFIER LBRACKET PIDENTIFIER RBRACKET
    { compiler.prepare_array_assign_var_index(*$1, *$3); }
    ASSIGN expression SEMICOLON
    {
      string* id = new string("@" + *$1);
      compiler.assign_var(*id);
      delete id;
      delete $3;
      delete $1;
    }
    | PIDENTIFIER LBRACKET NUM RBRACKET
    { compiler.prepare_array_assign_const_index(*$1, $3); }
    ASSIGN expression SEMICOLON
    {
      string* id = new string("@" + *$1);
      compiler.assign_var(*id);
      delete id;
      delete $1;
    }
    | IF condition THEN { compiler.if_start(); } commands optional_else ENDIF {
        compiler.if_end();
    }
    | WHILE condition DO { compiler.while_start(); } commands ENDWHILE {
        compiler.while_end();
    }
    | REPEAT { compiler.repeat_start(); } commands UNTIL condition SEMICOLON {
        compiler.repeat_end();
    }
    | FOR PIDENTIFIER FROM value TO value {
        compiler.for_start(*$2, *$4, *$6, "TO");
        delete $2; delete $4; delete $6;
    } DO commands ENDFOR {
        compiler.for_end();
    }
    | FOR PIDENTIFIER FROM value DOWNTO value {
        compiler.for_start(*$2, *$4, *$6, "DOWNTO");
        delete $2; delete $4; delete $6;
    } DO commands ENDFOR {
        compiler.for_end();
    }
    | proc_call SEMICOLON
    | READ identifier SEMICOLON {
        compiler.read(*$2);
        delete $2;
    }
    | WRITE value SEMICOLON {
        compiler.write(*$2);
        delete $2;
    }
;

optional_else:
    ELSE { compiler.if_else(); } commands
    | 
  ;

proc_call:
    PIDENTIFIER LPAREN args RPAREN {
        compiler.proc_call(*$1);
        delete $1;
    }
;

declarations:
    declarations COMMA PIDENTIFIER {
        compiler.declare_variable(*$3);
        delete $3;
    }
    | declarations COMMA PIDENTIFIER LBRACKET NUM COLON NUM RBRACKET {
        compiler.declare_array(*$3, $5, $7);
        delete $3;
    }
    | PIDENTIFIER {
        compiler.declare_variable(*$1);
        delete $1;
    }
    | PIDENTIFIER LBRACKET NUM COLON NUM RBRACKET {
        compiler.declare_array(*$1, $3, $5);
        delete $1;
    }
;

args_decl:
    args_decl COMMA type PIDENTIFIER {
        compiler.arg_decl(*$3, *$4); 
        delete $3; delete $4;
    }
    | type PIDENTIFIER {
        compiler.arg_decl(*$1, *$2);
        delete $1; delete $2;
    }
    |
;

type:
    KW_T { $$ = new string("T"); }
    | KW_I { $$ = new string("I"); }
    | KW_O { $$ = new string("O"); }
    |  { $$ = new string(""); } 
;

args:
    args COMMA PIDENTIFIER { 
        compiler.arg_use(*$3);
        delete $3;
    }
    | PIDENTIFIER { compiler.arg_use(*$1); delete $1; }
    | 
;

expression:
    value {
        compiler.load_value(*$1);
        delete $1;
    }
    | value PLUS value {
        compiler.add(*$1, *$3);
        delete $1; delete $3;
    }
    | value MINUS value {
        compiler.sub(*$1, *$3);
        delete $1; delete $3;
    }
    | value MULT value  {
        compiler.mul(*$1, *$3);
        delete $1; delete $3;
    }
    | value DIV value   {
        compiler.div(*$1, *$3);
        delete $1; delete $3;
    }
    | value MOD value   {
        compiler.mod(*$1, *$3);
        delete $1; delete $3;
    }
;

condition:
    value EQ value { compiler.condition("EQ", *$1, *$3); delete $1; delete $3; }
    | value NEQ value { compiler.condition("NEQ", *$1, *$3); delete $1; delete $3; }
    | value GT value { compiler.condition("GT", *$1, *$3); delete $1; delete $3; }
    | value LT value { compiler.condition("LT", *$1, *$3); delete $1; delete $3; }
    | value GEQ value { compiler.condition("GEQ", *$1, *$3); delete $1; delete $3; }
    | value LEQ value { compiler.condition("LEQ", *$1, *$3); delete $1; delete $3; }
;

value:
    NUM { $$ = new string(to_string($1)); }
    | identifier { $$ = $1; }
;

identifier:
    PIDENTIFIER { $$ = $1; }
    | PIDENTIFIER LBRACKET PIDENTIFIER RBRACKET {
        compiler.array_access_var_index(*$1, *$3);    
        long long tmp = compiler.capture_array_value_to_temp();
        $$ = new string("#" + to_string(tmp));
        delete $3;
        delete $1;
    }
    | PIDENTIFIER LBRACKET NUM RBRACKET {
        compiler.array_access_const_index(*$1, $3); 
        long long tmp = compiler.capture_array_value_to_temp();
        $$ = new string("#" + to_string(tmp));
        delete $1;
    }
;

%%

void yyerror(const char *s) {
    cerr << "Błąd składni w linii " << yylineno << ": " << s << endl;
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "Użycie: ./kompilator <plik_wejsciowy> <plik_wyjsciowy>" << endl;
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        cerr << "Nie można otworzyć pliku: " << argv[1] << endl;
        return 1;
    }

    yyparse();
    fclose(yyin);

    ofstream outfile(argv[2]);
    compiler.finish(outfile);
    outfile.close();

    cout << "Kompilacja zakończona." << endl;
    return 0;
}
