/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <linux/if_arp.h>

#include "berkelib/macros_.h"
#include "berkelib/utils_.h"
#include "resource_usage_tracker.h"

/*
*	Globals
*/
#ifdef DEBUG_RUT
extern enum program_flags			Program_Flag	= pf_No_CLI_Output;
#else
extern enum program_flags			Program_Flag	= pf_None;
#endif
extern enum program_states 			Program_State 	= ps_Ready;
extern enum initialization_states 	Init_State 		= is_None;

pthread_mutex_t Cpu_Mutex = PTHREAD_MUTEX_INITIALIZER;

/*
*	Functions
*/

// Get CPU's snapshot
void getCpuTimings(uint32_t *cpu_total, uint32_t *cpu_idle, REQUIRE_WITH_SIZE(char *, cpu_identifier)){

	*cpu_total = 0;
	*cpu_idle = 0;

	const char delim[2] = " ";
	uint8_t i = 0;
	char 	*token 	= NULL,
			*temp 	= NULL;

	readSearchGetFirstLine(&temp, PATH_CPU_STATS, PASS_WITH_SIZE_VAR(cpu_identifier), 1, PASS_WITH_SIZEOF(" "));

	token = strtok(temp, delim);
	token = strtok(NULL, delim); // skip cpu_id
	free(temp);

	while ( token != NULL){

		*cpu_total += atol(token);
		token = strtok(NULL, delim);

		if (++i == 3){

			if (token) {

				*cpu_idle = atol(token);

			} else {

				*cpu_total = 0;
				*cpu_idle = 0;

				printf( RED_BOLD("[ERROR]") " Could NOT parse: %s\n", "getCpuTimings");
				return;
			}

		}

	}// while
}

// 1000 ms is stable
// cat <(grep cpu /proc/stat) <(sleep 0.1 && grep cpu /proc/stat) | awk -v RS="" '{printf "%.1f", ($13-$2+$15-$4)*100/($13-$2+$15-$4+$16-$5)}
void *getCpuUsage(void *interval_ptr){

/*
*	Test The Parameter
*/

	uint32_t interval = DEFAULT_GLOBAL_INTERVAL;
	
	if ( (int64_t *) interval_ptr >= 0 ){

		interval = *((uint32_t *) interval_ptr);

	} else {

		fprintf(STD, RED_BOLD("[ERROR] CPU Interval is lower than 0. It is set to %d.\n"), DEFAULT_GLOBAL_INTERVAL);

	}

/*
*	Declarations
*/

	uint32_t	total		= 0,	idle		= 0,
				prev_total	= 0,	prev_idle	= 0;

	float 		usage = 0;

/*
*	Initialize CPU Timings
*/

	pthread_mutex_lock(&Cpu_Mutex);

	getCpuTimings(&total, &idle, PASS_WITH_SIZEOF("cpu"));
	sleep_ms(interval);

	Init_State |= is_Cpu;

	pthread_mutex_unlock(&Cpu_Mutex);

/*
*	Get CPU Usage Till The Program Stops
*/

	if( Init_State & is_Cpu ){

		while( Program_State & ps_Running ){

			pthread_mutex_lock(&Cpu_Mutex);

			prev_idle 	= idle;
			prev_total 	= total;

			getCpuTimings(&total, &idle, PASS_WITH_SIZEOF("cpu"));
		
			usage = (float) (1.0 - (long double) (idle-prev_idle) / (long double) (total-prev_total) ) * 100.0;
			
			// Output
			if ( !(Program_Flag & pf_No_CLI_Output) ){

				CONSOLE_GOTO(0, 0);
				fprintf(STD, "CPU Usage: %2.2f%%\n", usage);
				fflush(STD);

			}

#ifdef DEBUG_RUT
			fprintf(STD,
				CYAN_BOLD(" --- getCpuUsage()\n")	\
					PR_VAR("d", interval)		\
					PR_VAR("2.2f", usage)			\
				CYAN_BOLD(" ---\n") 				\
				, interval, usage
			);
#endif
			// ---

			pthread_mutex_unlock(&Cpu_Mutex);

			sleep_ms(interval);

		}

		return NULL;

	} else {

		fprintf(STD, RED_BOLD("[ERROR]") " CPU Timings are not initialized.\n");
		return NULL;

	}

	return NULL;
}

// Find the first VARIABLE in PATH and return its numeric value
uint64_t getFirstVarNumValue( const char* path, REQUIRE_WITH_SIZE(const char*, variable), const uint16_t variable_column_no ){
	uint64_t output = 0;
	const char delim[2] = " ";
	char 	*token 	= NULL,
			*temp 	= NULL;

	readSearchGetFirstLine(&temp, path, PASS_WITH_SIZE_VAR(variable), variable_column_no, PASS_WITH_SIZEOF(delim));

	token = strtok(temp, delim);
	token = strtok(NULL, delim); // skip VARIABLE
	

	if (token) {
		output = atoll(token) * KILOBYTE; // Convert to BYTE
	} else {
		fprintf(STD, RED_BOLD("[ERROR]") " Could NOT parse the value of: "RED_BOLD("%s")"\n", variable);
		return 0;
	}
#ifdef DEBUG_RUT
	fprintf(STD, CYAN_BOLD("getFirstVarNumValue()")" | %s = %llu\n", variable, output);
#endif
	free(temp);
	return output;
}

// Get the current OS disk
char* getSystemDisk(char* os_partition_name, char* maj_no){
	const char disk_min_no = '0';
	
	os_partition_name = NULL;
	maj_no = NULL;
	
	char 	*output = NULL,
			*temp 	= NULL;
SOUT("s", temp); SOUT("s", os_partition_name); TEST
	readSearchGetFirstLine(&temp, PATH_MOUNT_STATS, PASS_WITH_SIZEOF("/"), 5, PASS_WITH_SIZEOF(" "));

	getColumn(
		&os_partition_name,
		temp,
		2,
		PASS_WITH_SIZEOF(" ")
	);
SOUT("s", temp); SOUT("s", os_partition_name); TEST
	free(temp);
	temp = (char *) malloc (0);
SOUT("s", temp); SOUT("s", maj_no); TEST
	const uint16_t os_partition_name_size = leftTrimTill(os_partition_name, '/'); // Trim path
	readSearchGetFirstLine(&temp, PATH_DISK_STATS, PASS_WITH_SIZE_VAR(os_partition_name), 3, PASS_WITH_SIZEOF(" "));

	getColumn(
		&maj_no,
		temp,
		1,
		PASS_WITH_SIZEOF(" ")
	);
SOUT("s", temp); SOUT("s", maj_no); TEST
	free(temp);
	temp = (char *) malloc (0);
SOUT("s", temp); SOUT("s", output); TEST
	readSearchGetFirstLine(&temp, PATH_DISK_STATS, PASS_WITH_SIZEOF(maj_no), 1, PASS_WITH_SIZEOF(" "));

	getColumn(
		&output,
		temp,
		3,
		PASS_WITH_SIZEOF(" ")
	);
SOUT("s", temp); SOUT("s", output); TEST
	free(temp);

#ifdef DEBUG_RUT
	fprintf(STD, CYAN_BOLD("getSystemDisk()")" | output = %s | os_partition_name = %s | maj_no = %s\n", output, os_partition_name, maj_no);
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
	char ***read_write 	= ( char *** ) 	malloc ( sizeof(char **) * 2 );
	
	str_split(read_write, run_command(input_cmd), '\t', &temp_size);

	sleep_ms(ms_interval);

	str_split((read_write + 1), run_command(input_cmd), '\t', &temp_size);
	free(input_cmd);

	*bread_sec_out = atoll(read_write[1][0]) - atoll(read_write[0][0]); // BR - AR
	*bwrite_sec_out = atoll(read_write[1][1]) - atoll(read_write[0][1]); // BW - AW

#ifdef DEBUG_RUT
	fprintf(STD,
		CYAN_BOLD(" --- getDiskReadWrite(")"%s"CYAN_BOLD(") ---\n") \
			PR_VAR("d", disk_name_size) 	\
			PR_VAR("d", ms_interval)		\
			PR_VAR("s", read_write[0][0])	\
			PR_VAR("s", read_write[0][1])	\
			PR_VAR("s", read_write[1][0])	\
			PR_VAR("s", read_write[1][1])	\
			PR_VAR("d", *bread_sec_out)		\
			PR_VAR("d", *bwrite_sec_out)	\
		CYAN_BOLD(" ---\n") 				\
		, disk_name, disk_name_size, ms_interval, read_write[0][0], read_write[0][1], read_write[1][0], read_write[1][1], *bread_sec_out, *bwrite_sec_out
	);

#endif
	free(read_write);
}

// Get all disks from PATH_DISK_STATS
// Get disks names and maj if minor no is == 0
// $ cat "PATH_DISK_STATS" | awk '$2 == 0 {print $3}'
void getAllDisks(disks_t *disks){
	char **temp = NULL;

	str_split(&temp , run_command("awk '$2 == 0 {print $3}' "PATH_DISK_STATS), '\n', (size_t *) &disks->count);

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
	fprintf(STD, CYAN_BOLD(" --- getAllDisks() ---\n"));
	SOUT("d", disks->count);
	fprintf(STD, CYAN_BOLD("Disk Names:\n"));
	for(uint16_t i = 0 ; i < disks->count; i++){
		fprintf(STD, CYAN_BOLD("-")"%s\n", (*((*disks).info + i)).name);
	}
	fprintf(STD, CYAN_BOLD(" ---\n"));
#endif
}

// df --type btrfs --type ext4 --type ext3 --type ext2 --type vfat --type iso9660 --block-size=1 | tail -n +2 | awk {'print $1" "$2" "$3" "$4'}
void getPhysicalFilesystems(filesystems_t *filesystems){
	char **filesystems_temp = NULL;
	
	str_split(&filesystems_temp, run_command("df --type btrfs --type ext4 --type ext3 --type ext2 --type vfat --type iso9660 --block-size=1 | tail -n +2 | awk {'print $1\" \"$2\" \"$3\" \"$4'}"), '\n', (size_t *) &filesystems->count);

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
	char **fsi_temp = NULL;
	for (i = 0; i < filesystems->count; i++){
		(*(filesystems + i)).info = (struct filesystem_info *)malloc(sizeof(struct filesystem_info));
		str_split(&fsi_temp, *(filesystems_temp + i), ' ', &info_field_count );
		(*((*filesystems).info + i)).partition 	= *(fsi_temp + 0);
		(*((*filesystems).info + i)).block_size = (size_t) *(fsi_temp + 1); // as 1-byte sizes
		(*((*filesystems).info + i)).used 		= (size_t) *(fsi_temp + 2);
		(*((*filesystems).info + i)).available 	= (size_t) *(fsi_temp + 3);
	}
	free(fsi_temp);
	free(filesystems_temp);

	#ifdef DEBUG_RUT
		fprintf(STD, CYAN_BOLD(" --- getPhysicalFilesystems() ---\n"));
		SOUT("d", filesystems->count);
		fprintf(STD, CYAN_BOLD("Filesystems:\n"));
		for(uint16_t i = 0 ; i < filesystems->count; i++){
			fprintf(STD, CYAN_BOLD("- Partition: ")"\t%s\n", (*((*filesystems).info + i)).partition);
			fprintf(STD, CYAN_BOLD("- Byte blocks: ")"\t%s\n", (*((*filesystems).info + i)).block_size);
			fprintf(STD, CYAN_BOLD("- Used: ")"\t%s\n", (*((*filesystems).info + i)).used);
			fprintf(STD, CYAN_BOLD("- Available: ")"\t%s\n", (*((*filesystems).info + i)).available);
		}
		fprintf(STD, CYAN_BOLD(" ---\n"));
	#endif
}

//Alternative: tail -n +3 /proc/net/dev | awk '{print $1}' | sed 's/.$//'
void getNetworkInterfaces(net_ints_t *netints){

	// Get Interface List
	char **netints_temp = NULL;

	str_split(&netints_temp, run_command("ls /sys/class/net/"), '\n', (size_t *) &netints->count);

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
	FILE *net_file = NULL;

	uint16_t filtered_index = 0, arphdr_no = UINT16_MAX;
	for (uint16_t i = 0; i < netints->count; i++){
		// Get Type
		path = realloc(path, sizeof("/sys/class/net//type") + sizeof((*((*netints).info + i)).name));
		strcpy(path, "/sys/class/net/");
		strcat(path, (*((*netints).info + i)).name);
		strcat(path, "/type");

		net_file = fopen(path, "r");

		fscanf(net_file, "%d", &arphdr_no);

		// Discard if not an ARP Hardware interface
		if ( arphdr_no <= ARPHRD_INFINIBAND){

			// Assign to the list
			(*((*netints).info + filtered_index)).name = (*((*netints).info + i)).name;
			(*((*netints).info + filtered_index)).type = arphdr_no;

			// Get Bandwith
			path = realloc(path, sizeof("/sys/class/net//speed") + sizeof((*((*netints).info + filtered_index)).name));
			strcpy(path, "/sys/class/net/");
			strcat(path, (*((*netints).info + filtered_index)).name);
			strcat(path, "/speed");

			net_file = fopen(path, "r");

			fscanf(net_file, "%lu", &((*netints).info + i)->bandwith_mbps);
			filtered_index++;
		} else {
			(netints->count)--;
		}
	}
	free(path);
	fclose(net_file);

#ifdef DEBUG_RUT
	fprintf(STD, CYAN_BOLD(" --- getNetworkInterfaces() ---\n"));
	SOUT("d", netints->count);
	fprintf(STD, CYAN_BOLD("Network Interfaces:\n"));
	for(uint16_t i = 0 ; i < netints->count; i++){
		fprintf(STD, CYAN_BOLD("- Name:") " %s\n\t" CYAN_BOLD("Type:") " %d\n\t" CYAN_BOLD("Bandwith:") " %lu Mbps\n", (*((*netints).info + i)).name, (*((*netints).info + i)).type, (*((*netints).info + i)).bandwith_mbps);
	}
	fprintf(STD, CYAN_BOLD(" ---\n"));
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
void getNetworkIntUsage(struct net_int_info *netint, uint32_t interval){

	size_t temp_size;
	str_ptrlen(&temp_size, netint->name);
	char *input_cmd = (char *)malloc(
		temp_size * sizeof(char) + sizeof("tail -n +3 /proc/net/dev | grep | awk '{print $2\" \"$10}'")
	);

	strcpy(input_cmd, "tail -n +3 /proc/net/dev | grep ");
	strcat(input_cmd, netint->name);
	strcat(input_cmd, " | awk '{print $2\" \"$10}'");

	char ***down_up = (char ***)calloc(2, sizeof(char **));

	temp_size = 0;
	str_split(down_up, run_command(input_cmd), ' ', &temp_size);

	sleep_ms(interval);

	str_split(down_up + 1, run_command(input_cmd), ' ', &temp_size);

	free(input_cmd);

	netint->down_bps = atoll(down_up[1][0]) - atoll(down_up[0][0]); // AD - BD
	netint->up_bps = atoll(down_up[1][1]) - atoll(down_up[0][1]); // AU - BU

#ifdef DEBUG_RUT

	fprintf(STD,
		CYAN_BOLD(" --- getNetworkIntUsage(") "%s" CYAN_BOLD(") ---\n") \
			PR_VAR("d", interval)			\
			PR_VAR("s", down_up[0][0])		\
			PR_VAR("s", down_up[0][1])		\
			PR_VAR("s", down_up[1][0])		\
			PR_VAR("s", down_up[1][1])		\
			PR_VAR("d", netint->down_bps)	\
			PR_VAR("d", netint->up_bps)		\
		CYAN_BOLD(" ---\n") 				\
		, netint->name, interval, down_up[0][0], down_up[0][1], down_up[1][0], down_up[1][1], netint->down_bps, netint->up_bps
	);

#endif
}