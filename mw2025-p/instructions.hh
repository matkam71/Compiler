/*
 * Instrukcje maszyny wirtualnej do projektu z JFTT2025
 *
 * Autor: Maciek Gębala
 * http://ki.pwr.edu.pl/gebala/
 * 2025-11-15
*/
#pragma once

enum Instructions : int { READ, WRITE, LOAD, STORE, RLOAD, RSTORE, ADD, SUB, SWP, RST, INC, DEC, SHL, SHR, JUMP, JPOS, JZERO, CALL, RTRN, HALT };
