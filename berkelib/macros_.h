/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#ifndef BERKELIB_MACROS__H
#define BERKELIB_MACROS__H

#ifndef STD
#define STD stderr
#endif

#define KILOBYTE 1024
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)
#define SECTOR (KILOBYTE / 2)

#define STR(X) 						#X
#define ADD_QUOTES(X) 				"\"" #X "\""
#define PASS_WITH_SIZEOF(X) 		X, sizeof(X)
#define PASS_WITH_SIZE_VAR(X) 		X, X##_size
#define REQUIRE_WITH_SIZE(TYPE, X) 	TYPE X, const size_t X## _size

#define RED_BOLD(X) 	"\033[1;31m" X "\033[0m"
#define CYAN_BOLD(X) 	"\033[1;34m" X "\033[0m"

#define PR_VAR(T, X)	CYAN_BOLD(#X) ": %" T "\n"
#define SOUT(T, X)		fprintf(STD, PR_VAR(T, X), X)

#define CONSOLE_RESET()     fprintf(STD, "\033[H\033[J")
#define CONSOLE_GOTO(X, Y)  fprintf(STD, "\033[%d;%dH", Y, X)

#endif