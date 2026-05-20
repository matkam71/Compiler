# Compiler
Imperative language compiler in C++

279770
Mateusz Kameduła

Pliki:
- lexer.l - analiza leksykalna (Flex)
- parser.y - analiza składniowa (Bison)
- compiler.h - definicje struktur i deklaracje funkcji
- compiler.cpp - implementacja kompilatora i generowanie kodu maszyny wirtualnej
- Makefile - kompilacja

kompilacja i uruchomienie:
make 
./kompilator program.imp program.mr

Wymagania:
- flex
- bison
- g++
Testowane na systemie Ubuntu
