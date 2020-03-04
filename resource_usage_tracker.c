#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define DEBUG_RUT

#define KILOBYTE 1024
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)

#define PATH_CPU_STATS "/proc/stat"
#define PATH_DISK_STATS "/proc/diskstats"
#define PATH_MEM_INFO "/proc/meminfo"

#define PASS_ARR(X) X, sizeof(X)
#define REQUEST_ARR(X) X[], const unsigned int key_size



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

// Read the first subsequent lines at PATH starting with SEARCH_KEY
char* readSearchFirstSubsequentLines(const char* path, REQUEST_ARR(const char search_key)){
	const uint16_t buffer_size = KILOBYTE;

	FILE *fp = fopen(path, "r");
	if (fp == NULL){
		return strcat("Could not read the file: ", path);
	}
	char *output = NULL, *line = NULL;
	size_t output_size = 0;
	output = (char *)malloc(output_size);
	line = (char *)malloc(buffer_size);
	*output = '\0';

	while( fgets(line, buffer_size, fp) != NULL ) {

		output_size += strptrlen(line) * sizeof(char);
		output = realloc(output, output_size);
		strcat(output, line);
		
		if ( !strncmp(line, search_key, key_size-1) )
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

	token = strtok(readSearchFirstSubsequentLines(PATH_CPU_STATS, PASS_ARR("cpu ")), delim);
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
uint64_t getFirstVarNumValue( const char* path, REQUEST_ARR(const char variable) ){
	uint64_t output = 0;
	const char delim[2] = " ";
	char* token;

	token = strtok(readSearchFirstSubsequentLines( path, variable, key_size ), delim);
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

int main (){
	uint_fast16_t interval = 1000; // CHANGE IT TO 1000
	printf("\n");

	getCpuUsage(interval); // <-- Run this first to synchronize after sleep
	getFirstVarNumValue(PATH_MEM_INFO, PASS_ARR("Active"));
	getFirstVarNumValue(PATH_MEM_INFO, PASS_ARR("MemTotal"));

	return (EXIT_SUCCESS);
}