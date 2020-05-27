/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#ifndef BERKELIB_UTILS__H
#define BERKELIB_UTILS__H

#include <stdint.h>
#include <stddef.h>

#include "macros_.h"

void        str_ptrlen              (size_t *output, char *str);
void        str_to_int64            (char *string, int64_t *output);
void        str_to_int32            (char *string, int32_t *output) ;
void        str_to_uint64           (char *string, uint64_t *output);
void        str_to_uint32           (char *string, uint32_t *output);
void        str_to_float            (char *string, float *output);
void 	    str_split				(char *** output, char *str, const char delimiter, size_t *count_ptr);
char *      bytes_to_str            (size_t bytes);

void 		sleep_ms				(const uint32_t milliseconds);
char * 		run_command				(char *command);
size_t 	    leftTrimTill			(char *strptr, const char ch);
void 		getColumn				(char **output, char *str, const uint16_t column_no, REQUIRE_WITH_SIZE(const char *, delim));
void 		readSearchGetFirstLine	(char **output, const char *path, REQUIRE_WITH_SIZE(const char *, search_key), const uint16_t search_column, REQUIRE_WITH_SIZE(const char*, delim));

#endif