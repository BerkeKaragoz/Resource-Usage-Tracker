#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STRLEN(X) sizeof(X)-1

#define FILENAME "/proc/stat"

void sleep_ms(int milliseconds) 
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

char* readCpuStat(const char cpu_d[]){
	const char cpu_id[5] = "cpu ";
	const unsigned int buffer_size = 1024;

	FILE *fp = fopen(FILENAME, "r");
	if (fp == NULL){
		return strcat("Could not read the file: ", FILENAME);
	}
	char *output = NULL, *line = NULL;
	size_t output_size = buffer_size;
	output = (char *)malloc(output_size);
	line = (char *)malloc(buffer_size);
	*output = '\0';

	while( fgets(line, buffer_size, fp) != NULL && !strncmp(line, cpu_id, STRLEN(cpu_id)) ) {
		output_size *= 2;
		output = realloc(output, output_size);
		strcat(output, line);
	}
	fclose(fp);
	return output;
}

void getCpuTimings(unsigned long *cpu_total, unsigned long *cpu_idle){
	*cpu_total = 0;
	*cpu_idle = 0;

	const char delim[2] = " ";
	unsigned char i = 0;
	char* token;

	token = strtok(readCpuStat("cpu "), delim);
	token = strtok(NULL, delim); // skip cpu_id

	while ( token != NULL){
		*cpu_total += atol(token);
		token = strtok(NULL, delim);
		if (++i == 3){
			*cpu_idle = atol(token);
		}
	}

}

// 1000 ms is stable
long double getCpuUsage(const unsigned int ms_interval){

	unsigned long 	total = 0, idle = 0,
					prev_total = 0, prev_idle = 0;
	long double 	cpu_idle_fraction = 0, usage = 0;

	getCpuTimings(&prev_total, &prev_idle);
	sleep_ms(ms_interval);
	getCpuTimings(&total, &idle);

	usage = (1.0 - (long double) (idle-prev_idle) / (long double) (total-prev_total) ) * 100.0;

	printf("\nCPU Usage: %2.2Lf%%\n", usage);

	return usage;
}

int main (){
	const int interval = 100;

	getCpuUsage(interval);

	return 0;
}

// cat <(grep cpu /proc/stat) <(sleep 0.1 && grep cpu /proc/stat) | awk -v RS="" '{printf "%.1f", ($13-$2+$15-$4)*100/($13-$2+$15-$4+$16-$5)}
// cat <(grep cpu /proc/stat) <(sleep 0.1 && grep cpu /proc/stat) | awk -v RS="" '{printf "%.1f\t%.1f\n", $13, $2}'
