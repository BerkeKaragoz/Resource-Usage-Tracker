/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "macros_.h"
#include "utils_.h"


// Returns *STR's lenght 
void str_ptrlen(size_t *output, char *str){
	char *tmp = str;
	*output = 0;
	// Count STR's lenght
	while(*tmp++) (*output)++;
	return;
}

void str_to_int64(char *string, int64_t *output) {
	char *temp;
	*output = strtoll(string, &temp, 0);

	if (*temp != '\0'){
		fprintf(stderr, RED_BOLD("[ERROR]") " %s to long and leftover string is: %s\n", string, temp);
		exit(EXIT_FAILURE);
	}
}

void str_to_int32(char *string, int32_t *output) {
	char *temp;
	*output = strtol(string, &temp, 0);

	if (*temp != '\0'){
		fprintf(stderr, RED_BOLD("[ERROR]") " %s to long and leftover string is: %s\n", string, temp);
		exit(EXIT_FAILURE);
	}
}

void str_to_uint64(char *string, uint64_t *output) {
	char *temp;
	*output = strtoull(string, &temp, 0);

	if (*temp != '\0'){
		fprintf(stderr, RED_BOLD("[ERROR]") " %s to long and leftover string is: %s\n", string, temp);
		exit(EXIT_FAILURE);
	}
}

void str_to_uint32(char *string, uint32_t *output) {
	char *temp;
	*output = strtoul(string, &temp, 0);

	if (*temp != '\0'){
		fprintf(stderr, RED_BOLD("[ERROR]") " %s to long and leftover string is: %s\n", string, temp);
		exit(EXIT_FAILURE);
	}
}

void str_to_float(char *string, float *output) {
	char *temp;
	*output = strtof(string, &temp);

	if (*temp != '\0'){
		fprintf(stderr, RED_BOLD("[ERROR]") " %s to long and leftover string is: %s\n", string, temp);
		exit(EXIT_FAILURE);
	}
}

// Splits STR by the DELIMITER to string array and returns it with element COUNT
void str_split(char ***output, char *str, const char delimiter, size_t *count_ptr){	
	
	char *tmp        = str;
    char *last_comma = 0;

	char delim[2];
    delim[0] 			= delimiter;
    delim[1] 			= 0;

	*output    	= 0;
	*count_ptr 	= 0;

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
	size_t size = strlen(str);
    *count_ptr += last_comma < (str + size - 1);

    // Add space for terminating null string so caller
    // knows where the list of returned strings ends.
    (*count_ptr)++;
	*(str + size - 1) = '\0';

    *output = malloc(sizeof(char*) * (*count_ptr));

    if (*output)
    {
        size_t idx  = 0;
        char* token = strtok(str, delim);

        while (token)
        {
            assert(idx < *count_ptr);
            *(*output + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == *count_ptr - 1);
        *(*output + idx) = 0;
    }

    return;
}

char * bytes_to_str(size_t bytes)
{
	static const char *sizes[] = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };

    char     *result = (char *) malloc(sizeof(char) * 20);
    size_t  multiplier = (size_t) EXBIBYTE;
    int i;

    for (i = 0; i < (sizeof(sizes)/sizeof(*(sizes))); i++, multiplier /= KILOBYTE)
    {   
        if (bytes < multiplier)
            continue;
        if (bytes % multiplier == 0)
            sprintf(result, "%zu %s", bytes / multiplier, sizes[i]);
        else
            sprintf(result, "%.1f %s", (float) bytes / multiplier, sizes[i]);
        return result;
    }
    strcpy(result, "0");
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
		void *temp = realloc(output, output_size);

		if(temp != NULL)
			output = (char *) temp;
		else {
			output = NULL;
			fprintf(stderr, RED_BOLD("[ERROR]") " Cannot reallocate memory\n");
		}

		strcat(output, path);
	}

	pclose(fp);
	return output;
}

size_t leftTrimTill(char *strptr, const char ch){
	const char *ptr = NULL;
	uint16_t index = 0;
	do {
		ptr = strchr(strptr+index, '/');
		if(ptr) {
			index = ptr - strptr;
			*(strptr + index) = 2; // STX
		}
	} while(ptr != NULL);

	size_t output_len;
	str_ptrlen(&output_len, strptr);

	memmove(strptr, strptr+index+1, output_len);

	return output_len - index - 1;
}

void getColumn(char **output, char* str, const uint16_t column_no, REQUIRE_WITH_SIZE(const char*, delim)){

	char *gctemp = NULL;
	
	gctemp = strtok(str, delim);
	for (uint16_t i = 1; i < column_no ; i++){
		gctemp = strtok(NULL, delim);
	}

	size_t temp_size;
	str_ptrlen(&temp_size, gctemp);
	*output = realloc(*output, temp_size);

	strcpy(*output, gctemp);
	return;
}

/*
* 	Read PATH
* 	Search for SEARCH_KEY at COLUMN (#)
* 	Where columns are seperated with DELIM
* 	Return the line where it is found 
*/
void readSearchGetFirstLine(char **output, const char* path, REQUIRE_WITH_SIZE(const char*, search_key), const uint16_t search_column, REQUIRE_WITH_SIZE(const char*, delim)){
	const uint16_t buffer_size = KILOBYTE;

	FILE *fp = fopen(path, "r");
	if (fp == NULL){
		fprintf(stderr, RED_BOLD("[ERROR]") " Could not read the file: %s", path);
		return;
	}

	char	*line = NULL,
			*token = NULL;

	size_t output_size = 0;

	*output = realloc(*output, sizeof(char));
	line = (char *)malloc(buffer_size);
	**output = '\0';

	size_t temp_size = 0;
	uint16_t i;
	while( fgets(line, buffer_size, fp) != NULL ) {
		
		str_ptrlen(&temp_size, line);
		output_size += temp_size * sizeof(char);
		*output = realloc(*output, output_size);
		strcpy(*output, line);
		
		token = strtok(line, delim);
		for (i = 1; i < search_column; i++)
			token = strtok(NULL, delim);
		
		if ( !strcmp(token, search_key) )
		{
			fclose(fp);
			free(line);
			return;
		} else {
			*output = NULL;
			output_size = 0;
			*output = realloc(*output, output_size);
		}

	}
	fclose(fp);
	free(line);
	return;
}