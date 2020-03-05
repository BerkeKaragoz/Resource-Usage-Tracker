#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define DEBUG_RUT

#define INTERVAL 1000 // 1000 is consistent minimum for cpu

#define KILOBYTE 1024
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)

#define PATH_PROC 			"/proc/"
#define PATH_CPU_STATS		PATH_PROC "stat"
#define PATH_DISK_STATS 	PATH_PROC "diskstats"
#define PATH_MOUNT_STATS 	PATH_PROC "self/mountstats"
#define PATH_MEM_INFO 		PATH_PROC "meminfo"

#define PASS_WITH_SIZEOF(X) X, sizeof(X)
#define PASS_WITH_SIZE_VAR(X) X, X##_size
#define REQUIRE_WITH_SIZE(TYPE, X) TYPE X, const size_t X## _size
#define FUNCSOUT(X) printf(#X": %s\n", X);


// Returns STR's size 
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

// Read PATH
// Search for SEARCH_KEY at COLUMN (#)
// Where columns are seperated with DELIM
// Return the line where it is found 
char* readSearchGetLine(const char* path, REQUIRE_WITH_SIZE(const char*, search_key), const uint16_t column, REQUIRE_WITH_SIZE(const char*, delim)){
	const uint16_t buffer_size = KILOBYTE;
	char* token;

	FILE *fp = fopen(path, "r");
	if (fp == NULL){
		return strcat("Could not read the file: ", path);
	}

	char *output = NULL, *line = NULL;
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
		
		if ( !strncmp(token, search_key, search_key_size-1) )
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

	token = strtok(readSearchGetLine(PATH_CPU_STATS, PASS_WITH_SIZEOF("cpu"), 1, PASS_WITH_SIZEOF(" ")), delim);
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
	printf("CPU Usage: %2.2f%%\n", usage);
#endif
	return usage;
}

// Find the first VARIABLE in PATH and return its numeric value
uint64_t getFirstVarNumValue( const char* path, REQUIRE_WITH_SIZE(const char*, variable), const uint16_t variable_column_no ){
	uint64_t output = 0;
	const char delim[2] = " ";
	char* token;

	token = strtok(readSearchGetLine( path, PASS_WITH_SIZE_VAR(variable), variable_column_no, PASS_WITH_SIZEOF(delim) ), delim);
	token = strtok(NULL, delim); // skip VARIABLE

	if (token) {
		output = atoll(token) * KILOBYTE; // Convert to BYTE
	} else {
		printf("[ERROR] Could NOT parse the value of: %s\n", variable);
		return 0;
	}
#ifdef DEBUG_RUT
	printf("%s: %llu\n", variable, output);
#endif
	return output;
}

char* getBootDisk(){
	char* output = NULL;

	output = readSearchGetLine(PATH_MOUNT_STATS, PASS_WITH_SIZEOF("/boot/efi"), 5, PASS_WITH_SIZEOF(" "));

	return output;
}

int main (){
	printf("\n");

	getCpuUsage(INTERVAL); // <-- Run this first to synchronize after sleep
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("Active"), 0);
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("MemTotal"), 0);
	FUNCSOUT(getBootDisk());

	return (EXIT_SUCCESS);
}

// $ cat /proc/self/mountstats | grep /boot/efi | awk '{print $2}' | sed -e "s/\/dev\///g" -e "s/p.//g"