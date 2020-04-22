/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>

#include "macros_.h"
#include "utils_.h"

// Returns *STR's lenght 
size_t strptrlen(char *str){
	char *tmp = str;
	size_t str_lenght = 0;
	// Count STR's lenght
	while(*tmp++) str_lenght++;
	return str_lenght * sizeof(char);
}

// Splits STR by the DELIMITER to string array and returns it with element COUNT
char** str_split(char* str, const char delimiter, size_t *count_ptr){
    char** result    = 0;
    char* tmp        = str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = delimiter;
    delim[1] = 0;

	*count_ptr = 0;

    // Count how many elements will be extracted.
    while (*tmp)
    {
        if (delimiter == *tmp && *(tmp+1) != '\0')
        {
            (*count_ptr)++;
            last_comma = tmp;
        }
        tmp++;
    }

    // Add space for trailing token.
    *count_ptr += last_comma < (str + strlen(str) - 1);

    // Add space for terminating null string so caller
    // knows where the list of returned strings ends.
    (*count_ptr)++;

    result = malloc(sizeof(char*) * (*count_ptr));

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(str, delim);

        while (token)
        {
            assert(idx < *count_ptr);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == *count_ptr - 1);
        *(result + idx) = 0;
    }

    return result;
}

void sleep_ms(const uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// Executes sh COMMAND and returns the OUTPUT
char* run_command(char* command){
	FILE *fp;
	size_t output_size = sizeof(char);
	char path[1035];
	char *output = (char *)malloc(output_size);
	*output = '\0';

	fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to run command: %s\n", command);
		exit(EXIT_FAILURE);
	}

	while (fgets(path, sizeof(path), fp) != NULL) {
		output_size += sizeof(path);
		output = realloc(output, output_size);
		strcat(output, path);
	}

	pclose(fp);
	return output;
}

uint16_t leftTrimTill(char *strptr, const char ch){
	const char *ptr = NULL;
	uint16_t index = 0;
	do {
		ptr = strchr(strptr+index, '/');
		if(ptr) {
			index = ptr - strptr;
			*(strptr + index) = 2; // STX
		}
	} while(ptr != NULL);

	uint16_t output_len = strptrlen(strptr);
	memmove(strptr, strptr+index+1, output_len);

	return output_len - index - 1;
}

char* getColumn(char* str, const uint16_t column_no, REQUIRE_WITH_SIZE(const char*, delim)){
	char* token;

	token = strtok(str, delim);
	for (uint16_t i = 1; i < column_no ; i++){
		token = strtok(NULL, delim);
	}

	return token;
}

// Read PATH
// Search for SEARCH_KEY at COLUMN (#)
// Where columns are seperated with DELIM
// Return the line where it is found 
char* readSearchGetFirstLine(const char* path, REQUIRE_WITH_SIZE(const char*, search_key), const uint16_t search_column, REQUIRE_WITH_SIZE(const char*, delim)){
	const uint16_t buffer_size = KILOBYTE;

	FILE *fp = fopen(path, "r");
	if (fp == NULL){
		return strcat( RED_BOLD("[ERROR]") " Could not read the file: ", path);
	}

	char 	*output = NULL,
			*line = NULL,
			*token = NULL;

	size_t output_size = 0;

	output = (char *)malloc(output_size);
	line = (char *)malloc(buffer_size);
	
	*output = '\0';

	uint16_t i;
	while( fgets(line, buffer_size, fp) != NULL ) {
			
		output_size += strptrlen(line) * sizeof(char);
		output = realloc(output, output_size);
		strcat(output, line);

		token = strtok(line, delim);
		for (i = 1; i < search_column; i++)
			token = strtok(NULL, delim);
		
		if ( !strcmp(token, search_key) )
		{

			return output;
		} else {
			output = NULL;
			output_size = 0;
			output = realloc(output, output_size);
		}

	}
	fclose(fp);
	free(line);
	return output;
}