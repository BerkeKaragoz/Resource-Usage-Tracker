#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>

#define DEBUG_RUT

#define DEFAULT_GLOBAL_INTERVAL 1000 // 1000 is consistent minimum for cpu on physical machines, use higher for virtuals
#define MAX_THREADS 64

#define KILOBYTE 1024
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)
#define SECTOR (KILOBYTE / 2)

#define PATH_PROC 			"/proc/"
#define PATH_CPU_STATS		PATH_PROC "stat"
#define PATH_DISK_STATS 	PATH_PROC "diskstats"
#define PATH_MOUNT_STATS 	PATH_PROC "self/mountstats"
#define PATH_MEM_INFO 		PATH_PROC "meminfo"

#define RED_BOLD(X) 	"\033[1;31m"X"\033[0m"
#define CYAN_BOLD(X) 	"\033[1;34m"X"\033[0m"

#define STR(X) 						#X
#define ADD_QUOTES(X) 				"\""#X"\""
#define PASS_WITH_SIZEOF(X) 		X, sizeof(X)
#define PASS_WITH_SIZE_VAR(X) 		X, X##_size
#define REQUIRE_WITH_SIZE(TYPE, X) 	TYPE X, const size_t X## _size
#define SOUT(T, X)					fprintf(stderr, CYAN_BOLD(#X)": %"T"\n", X);


struct disk_info{
	char *name;
	size_t read_per_sec;
	size_t write_per_sec;
};

typedef struct disks{
	struct disk_info *info;
	uint16_t count;
}disks_t;

struct filesystem_info{
	char *partition;
	size_t block_size;
	size_t used;
	size_t available;
};

typedef struct filesystems{
	struct filesystem_info *info;
	uint16_t count;
}filesystems_t;

struct net_int_info{
	char *name;
	uint32_t type;
	size_t bandwith_mbps;
	size_t up_bps;
	size_t down_bps;
};

typedef struct network_interfaces{
	struct net_int_info *info;
	uint16_t count;
}net_ints_t;


// Globals
uint32_t Disk_Interval = DEFAULT_GLOBAL_INTERVAL;


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

// Get CPU's snapshot
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
float getCpuUsage(const uint32_t ms_interval){
	uint32_t 	total = 0, idle = 0,
				prev_total = 0, prev_idle = 0;
	float 		usage = 0;

	getCpuTimings(&prev_total, &prev_idle);
	sleep_ms(ms_interval);
	getCpuTimings(&total, &idle);

	usage = (float) (1.0 - (long double) (idle-prev_idle) / (long double) (total-prev_total) ) * 100.0;
#ifdef DEBUG_RUT
	fprintf(stderr, CYAN_BOLD(" --- getCpuUsage()\n"));
	fprintf(stderr, CYAN_BOLD("Interval")": %d\n", ms_interval);
	fprintf(stderr, CYAN_BOLD("Usage")": %2.2f%%\n", usage);
	fprintf(stderr, CYAN_BOLD(" ---\n"));
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
		fprintf(stderr, RED_BOLD("[ERROR]") " Could NOT parse the value of: "RED_BOLD("%s")"\n", variable);
		return 0;
	}
#ifdef DEBUG_RUT
	fprintf(stderr, CYAN_BOLD("getFirstVarNumValue()")" | %s = %llu\n", variable, output);
#endif
	return output;
}

// Get the current OS disk
char* getSystemDisk(char* os_partition_name, char* maj_no){
	const char disk_min_no = '0';
	os_partition_name = NULL;
	maj_no = NULL;
	
	char* output = NULL;
	
	os_partition_name = getColumn(
		readSearchGetFirstLine(PATH_MOUNT_STATS, PASS_WITH_SIZEOF("/"), 5, PASS_WITH_SIZEOF(" ")),
		2,
		PASS_WITH_SIZEOF(" ")
	);

	const uint16_t os_partition_name_size = leftTrimTill(os_partition_name, '/'); // Trim path

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
	fprintf(stderr, CYAN_BOLD("getSystemDisk()")" | output = %s | os_partition_name = %s | maj_no = %s\n", output, os_partition_name, maj_no);
#endif

	return output;
}

// $ awk '$3 == "<DISK_NAME>" {print $6"\t"$10}' /proc/diskstats
void getDiskReadWrite(const uint32_t ms_interval, REQUIRE_WITH_SIZE(char *, disk_name), size_t *bread_sec_out, size_t *bwrite_sec_out){
	char *input_cmd = (char *)malloc(
		disk_name_size * sizeof(char) + sizeof("awk '$3 == \"\" {print $6\"\\t\"$10}' /proc/diskstats")
	);

	strcpy(input_cmd, "awk '$3 == \"");
	strcat(input_cmd, disk_name);
	strcat(input_cmd, "\" {print $6\"\\t\"$10}' /proc/diskstats");

	size_t temp_size = 0;
	char ***read_write = (char ***)malloc(sizeof(char**) * 2);
	
	*read_write = str_split(run_command(input_cmd), '\t', &temp_size);

	sleep_ms(ms_interval);

	*(read_write + 1) = str_split(run_command(input_cmd), '\t', &temp_size);
	free(input_cmd);

	*bread_sec_out = atoll(read_write[1][0]) - atoll(read_write[0][0]); // BR - AR
	*bwrite_sec_out = atoll(read_write[1][1]) - atoll(read_write[0][1]); // BW - AW

#ifdef DEBUG_RUT
	fprintf(stderr, CYAN_BOLD(" --- getDiskReadWrite(")"%s"CYAN_BOLD(") ---\n"), disk_name);
	SOUT("s", disk_name);
	SOUT("d", disk_name_size);
	SOUT("d", ms_interval);
	SOUT("s", read_write[0][0]);// Before 	Read
	SOUT("s", read_write[0][1]);// Before 	Write
	SOUT("s", read_write[1][0]);// After 	Read
	SOUT("s", read_write[1][1]);// After 	Write
	SOUT("d", *bread_sec_out);
	SOUT("d", *bwrite_sec_out);
	fprintf(stderr, CYAN_BOLD(" ---\n"));
#endif
	free(read_write);
}

// Get all disks from PATH_DISK_STATS
// Get disks names and maj if minor no is == 0
// $ cat "PATH_DISK_STATS" | awk '$2 == 0 {print $3}'
void getAllDisks(disks_t *disks){
	char **temp = str_split( run_command("awk '$2 == 0 {print $3}' "PATH_DISK_STATS), '\n', (size_t *) &disks->count);

	// Delete if NULL
	if (*temp == NULL){
		*temp = *(temp + 1);
		(disks->count)--;
	}

	uint16_t i;
	for(i = 1; i < disks->count; i++){

		if( *(temp + i) == NULL ) {
			if (i + 1 < disks->count){		
				*(temp + i - 1) = *(temp + i + 1);
			}
			(disks->count)--;
		}
		
	}//for
	// LLUN fi eteleD

	for (i = 0; i < disks->count; i++){
		(*(disks + i)).info = (struct disk_info *)malloc(sizeof(struct disk_info));
		(*((*disks).info + i)).name = *(temp + i);
	}
	
	free(temp);

#ifdef DEBUG_RUT
	fprintf(stderr, CYAN_BOLD(" --- getAllDisks() ---\n"));
	SOUT("d", disks->count);
	fprintf(stderr, CYAN_BOLD("Disk Names:\n"));
	for(uint16_t i = 0 ; i < disks->count; i++){
		fprintf(stderr, CYAN_BOLD("-")"%s\n", (*((*disks).info + i)).name);
	}
	fprintf(stderr, CYAN_BOLD(" ---\n"));
#endif
}

// df --type btrfs --type ext4 --type ext3 --type ext2 --type vfat --type iso9660 --block-size=1 | tail -n +2 | awk {'print $1" "$2" "$3" "$4'}
void getPhysicalFilesystems(filesystems_t *filesystems){
	char **filesystems_temp = str_split( run_command("df --type btrfs --type ext4 --type ext3 --type ext2 --type vfat --type iso9660 --block-size=1 | tail -n +2 | awk {'print $1\" \"$2\" \"$3\" \"$4'}"), '\n', (size_t *) &filesystems->count);

	// Delete if NULL
	if (*filesystems_temp == NULL){
		*filesystems_temp = *(filesystems_temp + 1);
		(filesystems->count)--;
	}

	uint16_t i;
	for(i = 1; i < filesystems->count; i++){

		if( *(filesystems_temp + i) == NULL ) {
			if (i + 1 < filesystems->count){		
				*(filesystems_temp + i - 1) = *(filesystems_temp + i + 1);
			}
			(filesystems->count)--;
		}
		
	}//for
	// LLUN fi eteleD
	size_t info_field_count = 0;
	char **fsi_temp;
	for (i = 0; i < filesystems->count; i++){
		(*(filesystems + i)).info = (struct filesystem_info *)malloc(sizeof(struct filesystem_info));
		fsi_temp = str_split(*(filesystems_temp + i), ' ', &info_field_count );
		(*((*filesystems).info + i)).partition 	= *(fsi_temp + 0);
		(*((*filesystems).info + i)).block_size = (size_t) *(fsi_temp + 1); // as 1-byte sizes
		(*((*filesystems).info + i)).used 		= (size_t) *(fsi_temp + 2);
		(*((*filesystems).info + i)).available 	= (size_t) *(fsi_temp + 3);
	}
	free(fsi_temp);
	free(filesystems_temp);

	#ifdef DEBUG_RUT
		fprintf(stderr, CYAN_BOLD(" --- getPhysicalFilesystems() ---\n"));
		SOUT("d", filesystems->count);
		fprintf(stderr, CYAN_BOLD("Filesystems:\n"));
		for(uint16_t i = 0 ; i < filesystems->count; i++){
			fprintf(stderr, CYAN_BOLD("- Partition: ")"\t%s\n", (*((*filesystems).info + i)).partition);
			fprintf(stderr, CYAN_BOLD("- Byte blocks: ")"\t%s\n", (*((*filesystems).info + i)).block_size);
			fprintf(stderr, CYAN_BOLD("- Used: ")"\t%s\n", (*((*filesystems).info + i)).used);
			fprintf(stderr, CYAN_BOLD("- Available: ")"\t%s\n", (*((*filesystems).info + i)).available);
		}
		fprintf(stderr, CYAN_BOLD(" ---\n"));
	#endif
}

void *call_getDiskReadWrite(void* disk_info_ptr){

	struct disk_info *dip = (struct disk_info *) disk_info_ptr;
	
	getDiskReadWrite(Disk_Interval, PASS_WITH_SIZEOF(dip->name), &(dip->read_per_sec), &(dip->write_per_sec));

	return NULL;
}

void *call_getCpuUsage(void *interval_ptr){
	
	uint32_t interval = DEFAULT_GLOBAL_INTERVAL;
	
	if ( (int64_t *) interval_ptr >= 0 ){
		interval = *((uint32_t *) interval_ptr);
	} else {
		fprintf(stderr, RED_BOLD("[ERROR] CPU Interval is lower than 0. It is set to %d.\n"), DEFAULT_GLOBAL_INTERVAL);
	}

	getCpuUsage(interval);

	return NULL;
}

//Alternative: tail -n +3 /proc/net/dev | awk '{print $1}' | sed 's/.$//'
void getNetworkInterfaces(net_ints_t *netints){

	// Get Interface List
	char **netints_temp = str_split( run_command("ls /sys/class/net/"), '\n', (size_t *) &netints->count);

	// Delete if NULL
	if (*netints_temp == NULL){
		*netints_temp = *(netints_temp + 1);
		(netints->count)--;
	}

	uint16_t i;
	for(i = 1; i < netints->count; i++){

		if( *(netints_temp + i) == NULL ) {
			if (i + 1 < netints->count){		
				*(netints_temp + i - 1) = *(netints_temp + i + 1);
			}
			(netints->count)--;
		}
		
	}//for
	// LLUN fi eteleD

	for (uint16_t i = 0; i < netints->count; i++){
		(*(netints + i)).info = (void *)malloc(sizeof(struct net_int_info));
		(*((*netints).info + i)).name = *(netints_temp + i);
	}

	free(netints_temp);

	// Get Specs
	char *path = NULL;
	FILE *bandwith_file = NULL;

	for (uint16_t i = 0; i < netints->count; i++){
		// Get Bandwith
		path = realloc(path, sizeof("/sys/class/net//speed") + sizeof((*((*netints).info + i)).name));
		strcpy(path, "/sys/class/net/");
		strcat(path, (*((*netints).info + i)).name);
		strcat(path, "/speed");

		bandwith_file = fopen(path, "r");

		fscanf(bandwith_file, "%lu", &((*netints).info + i)->bandwith_mbps);

		// Get Type
		path = realloc(path, sizeof("/sys/class/net//type") + sizeof((*((*netints).info + i)).name));
		strcpy(path, "/sys/class/net/");
		strcat(path, (*((*netints).info + i)).name);
		strcat(path, "/type");

		bandwith_file = fopen(path, "r");

		fscanf(bandwith_file, "%d", &((*netints).info + i)->type);
	}
	free(path);
	fclose(bandwith_file);

#ifdef DEBUG_RUT
	fprintf(stderr, CYAN_BOLD(" --- getNetworkInterfaces() ---\n"));
	SOUT("d", netints->count);
	fprintf(stderr, CYAN_BOLD("Network Interfaces:\n"));
	for(uint16_t i = 0 ; i < netints->count; i++){
		fprintf(stderr, CYAN_BOLD("- Name:") " %s\n\t" CYAN_BOLD("Type:") " %d\n\t" CYAN_BOLD("Bandwith:") " %lu Mbps\n", (*((*netints).info + i)).name, (*((*netints).info + i)).type, (*((*netints).info + i)).bandwith_mbps);
	}
	fprintf(stderr, CYAN_BOLD(" ---\n"));
#endif
}

/*
Get Bandwith
cat /sys/class/net/eth0/speed

Get Network Usage
awk '{if(l1){print $2-l1,$10-l2} else{l1=$2; l2=$10;}}' \
<(grep NET_INT_NAME /proc/net/dev) <(sleep 1; grep NET_INT_NAME /proc/net/dev)
*/
//tail -n +3 /proc/net/dev | grep NET_INT_NAME | column -t
void getNetworkUsage(net_ints_t *netints){
#ifdef DEBUG_RUT
	fprintf(stderr, CYAN_BOLD(" --- getNetworkUsage(")CYAN_BOLD(") ---\n"));
#endif

	for(uint16_t i = 0; i < netints->count; i++){
		char *input_cmd = (char *)malloc( //todo better strptrlen
			strptrlen((netints->info + i)->name) * sizeof(char) + sizeof("tail -n +3 /proc/net/dev | grep ")
		);

		strcpy(input_cmd, "tail -n +3 /proc/net/dev | grep ");
		strcat(input_cmd, (netints->info + i)->name);
		strcat(input_cmd, "| awk '{print $2\" \"$10}'");

		char ***down_up = (char ***)malloc(sizeof(char**) * 2);

		size_t temp_size = 0;
		*down_up = str_split(run_command(input_cmd), ' ', &temp_size);

		sleep_ms(DEFAULT_GLOBAL_INTERVAL);

		*(down_up + 1) = str_split(run_command(input_cmd), ' ', &temp_size);

		free(input_cmd);

		(netints->info + i)->down_bps = atoll(down_up[1][0]) - atoll(down_up[0][0]); // AD - BD
		(netints->info + i)->up_bps = atoll(down_up[1][1]) - atoll(down_up[0][1]); // AU - BU

#ifdef DEBUG_RUT
	fprintf(stderr, CYAN_BOLD(" - \n"));

	SOUT("s", (netints->info + i)->name);
	SOUT("d", DEFAULT_GLOBAL_INTERVAL);
	SOUT("s", down_up[0][0]);// Before 	Read
	SOUT("s", down_up[0][1]);// Before 	Write
	SOUT("s", down_up[1][0]);// After 	Read
	SOUT("s", down_up[1][1]);// After 	Write
	SOUT("d", (netints->info + i)->down_bps);
	SOUT("d", (netints->info + i)->up_bps);

	fprintf(stderr, CYAN_BOLD(" - \n"));
#endif
	}//for
}

//todo: multithread safe str_split runcmd
int main (){
#ifdef DEBUG_RUT
	clock_t _begin = clock();
#endif
	fprintf(stderr, "\n");
	pthread_t *disk_io_threads;
	pthread_t cpu_thread; 

	disks_t disks;
	filesystems_t filesystems;
	net_ints_t netints;

	uint32_t cpu_interval;
	SOUT("s", run_command("cat /proc/net/arp"));
	//RAM
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("Active:"), 0);
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("MemTotal:"), 0);

	getAllDisks(&disks); //Disks
	getPhysicalFilesystems(&filesystems); //Filesystems
	getNetworkInterfaces(&netints); //Network Interfaces
	getNetworkUsage(&netints);

	if(disks.count > 1)
	{
		getSystemDisk(NULL, NULL); //System Disk
	}
	else if (disks.count <= 0)
	{
		fprintf(stderr, RED_BOLD("[ERROR]")" Could not get the disks!\n");
		exit(EXIT_FAILURE);
	}
	
	if (disks.count > MAX_THREADS)
	{
		fprintf(stderr, RED_BOLD("[ERROR]")" Disk count exceeded maximum amount of threads!\n");
		exit(EXIT_FAILURE);
	}

	cpu_interval = DEFAULT_GLOBAL_INTERVAL;
	pthread_create(&cpu_thread, NULL, call_getCpuUsage, &cpu_interval); // CPU

	disk_io_threads = malloc(disks.count * sizeof(pthread_t)); 

	for (uint16_t i = 0; i < disks.count; i++){ // Disks Reads/Writes
		pthread_create(disk_io_threads + i, NULL, call_getDiskReadWrite, disks.info + i);
	}

	for (uint16_t i = 0; i < disks.count; i++){ // Join Thread Disks
		pthread_join(*(disk_io_threads + i), NULL);
	}

	pthread_join(cpu_thread, NULL); // Join Thread CPU
	
	pthread_exit(NULL);
	free(disk_io_threads);


#ifdef DEBUG_RUT
	clock_t _end = clock();
	fprintf(stderr, "\nExecution time: %lf\n", (double)(_end - _begin) / CLOCKS_PER_SEC);
#endif
	return (EXIT_SUCCESS);
}

// $ cat /sys/block/<disk>/device/model ---> KBG30ZMV256G TOSHIBA