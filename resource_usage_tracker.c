#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define DEBUG_RUT

#define INTERVAL 1 // 1000 is consistent minimum for cpu

#define RED_BOLD(X) 	"\033[1;31m"X"\033[0m"
#define CYAN_BOLD(X) 	"\033[1;34m"X"\033[0m"

#define KILOBYTE 1024
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)

#define PATH_PROC 			"/proc/"
#define PATH_CPU_STATS		PATH_PROC "stat"
#define PATH_DISK_STATS 	PATH_PROC "diskstats"
#define PATH_MOUNT_STATS 	PATH_PROC "self/mountstats"
#define PATH_MEM_INFO 		PATH_PROC "meminfo"

#define PASS_WITH_SIZEOF(X) 		X, sizeof(X)
#define PASS_WITH_SIZE_VAR(X) 		X, X##_size
#define REQUIRE_WITH_SIZE(TYPE, X) 	TYPE X, const size_t X## _size
#define SOUT(X) 					printf(CYAN_BOLD(#X)": %s\n", X);



// Returns STR's lenght 
size_t strptrlen(char *str){
	char *tmp = str;
	size_t str_lenght = 0;
	// Count STR's lenght
	while(*tmp++) str_lenght++;
	return str_lenght * sizeof(char);
}

void sleep_ms(uint16_t milliseconds) 
{
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
		exit(1);
	}

	while (fgets(path, sizeof(path), fp) != NULL) {
		output_size += sizeof(path);
		output = realloc(output, output_size);
		strcat(output, path);
	}

	pclose(fp);
	return output;
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
char* readSearchGetFirstLine(const char* path, REQUIRE_WITH_SIZE(const char*, search_key), const uint16_t column, REQUIRE_WITH_SIZE(const char*, delim)){
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
		for (i = 1; i < column; i++)
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
	return output;
}

void getCpuTimings(uint32_t *cpu_total, uint32_t *cpu_idle){
	*cpu_total = 0;
	*cpu_idle = 0;

	const char delim[2] = " ";
	uint8_t i = 0;
	char* token;

	token = strtok(readSearchGetFirstLine(PATH_CPU_STATS, PASS_WITH_SIZEOF("cpu"), 1, PASS_WITH_SIZEOF(" ")), delim);
	token = strtok(NULL, delim); // skip cpu_id

	while ( token != NULL){
		*cpu_total += atol(token);
		token = strtok(NULL, delim);
		if (++i == 3){
			if (token) {
				*cpu_idle = atol(token);
			} else {
				*cpu_total = 0;
				*cpu_idle = 0;
				printf("[ERROR] Could NOT parse: %s\n", "getCpuTimings");
				return;
			}
		}
	}
}

// 1000 ms is stable
// cat <(grep cpu /proc/stat) <(sleep 0.1 && grep cpu /proc/stat) | awk -v RS="" '{printf "%.1f", ($13-$2+$15-$4)*100/($13-$2+$15-$4+$16-$5)}
float getCpuUsage(const unsigned int ms_interval){
	uint32_t 	total = 0, idle = 0,
				prev_total = 0, prev_idle = 0;
	float 		usage = 0;

	getCpuTimings(&prev_total, &prev_idle);
	sleep_ms(ms_interval);
	getCpuTimings(&total, &idle);

	usage = (float) (1.0 - (long double) (idle-prev_idle) / (long double) (total-prev_total) ) * 100.0;
#ifdef DEBUG_RUT
	printf(CYAN_BOLD("getCpuUsage()")" | usage = %2.2f%%\n", usage);
#endif
	return usage;
}

// Find the first VARIABLE in PATH and return its numeric value
uint64_t getFirstVarNumValue( const char* path, REQUIRE_WITH_SIZE(const char*, variable), const uint16_t variable_column_no ){
	uint64_t output = 0;
	const char delim[2] = " ";
	char* token;

	token = strtok(readSearchGetFirstLine( path, PASS_WITH_SIZE_VAR(variable), variable_column_no, PASS_WITH_SIZEOF(delim) ), delim);
	token = strtok(NULL, delim); // skip VARIABLE

	if (token) {
		output = atoll(token) * KILOBYTE; // Convert to BYTE
	} else {
		printf(RED_BOLD("[ERROR]") " Could NOT parse the value of: "RED_BOLD("%s")"\n", variable);
		return 0;
	}
#ifdef DEBUG_RUT
	printf(CYAN_BOLD("getFirstVarNumValue()")" | %s = %llu\n", variable, output);
#endif
	return output;
}

uint16_t trimTill(char *strptr, const char ch){
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

char* getBootDisk(char* os_partition_name, char* maj_no){
	const char disk_min_no = '0';
	os_partition_name = NULL;
	maj_no = NULL;
	
	char* output = NULL;
	
	os_partition_name = getColumn(
		readSearchGetFirstLine(PATH_MOUNT_STATS, PASS_WITH_SIZEOF("/"), 5, PASS_WITH_SIZEOF(" ")),
		2,
		PASS_WITH_SIZEOF(" ")
	);

	const uint16_t os_partition_name_size = trimTill(os_partition_name, '/'); // Trim path

	maj_no = getColumn(
		readSearchGetFirstLine(PATH_DISK_STATS, PASS_WITH_SIZE_VAR(os_partition_name), 3, PASS_WITH_SIZEOF(" ")),
		1,
		PASS_WITH_SIZEOF(" ")
	);

	output = getColumn(
		readSearchGetFirstLine(PATH_DISK_STATS, PASS_WITH_SIZEOF(maj_no), 1, PASS_WITH_SIZEOF(" ")),
		3,
		PASS_WITH_SIZEOF(" ")
		);

#ifdef DEBUG_RUT
	printf(CYAN_BOLD("getBootDisk()")" | output = %s | os_partition_name = %s | maj_no = %s\n", output, os_partition_name, maj_no);
#endif

	return output;
}

int main (){
	printf("\n");

	getBootDisk(NULL, NULL);

	getCpuUsage(INTERVAL); // <-- Run this first to synchronize after sleep
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("Active:"), 0);
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("MemTotal:"), 0);

	return (EXIT_SUCCESS);
}

// $ cat /proc/self/mountstats | grep /boot/efi | awk '{print $2}' | sed -e "s/\/dev\///g" -e "s/p.//g"