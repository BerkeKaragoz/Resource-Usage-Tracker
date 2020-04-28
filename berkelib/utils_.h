/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#ifndef BERKELIB_UTILS__H
#define BERKELIB_UTILS__H

#include <stdint.h>
#include <stddef.h>

#include "macros_.h"

size_t 		strptrlen				(char *str);
void 	    str_split				(char *** output, char *str, const char delimiter, size_t *count_ptr);
void 		sleep_ms				(const uint32_t milliseconds);
char * 		run_command				(char *command);
uint16_t 	leftTrimTill			(char *strptr, const char ch);
char * 		getColumn				(char *str, const uint16_t column_no, REQUIRE_WITH_SIZE(const char *, delim));
char * 		readSearchGetFirstLine	(const char *path, REQUIRE_WITH_SIZE(const char *, search_key), const uint16_t search_column, REQUIRE_WITH_SIZE(const char*, delim));

#endif