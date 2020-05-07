/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#ifndef RESOURCE_USAGE_TRACKER_H
#define RESOURCE_USAGE_TRACKER_H

#include <stdint.h>
#include <stddef.h>

#include "berkelib/macros_.h"

#define DEBUG_RUT
#define DEFAULT_GLOBAL_INTERVAL 1000 // 1000 is consistent minimum for cpu on physical machines, use higher for virtuals
#define MAX_THREADS 64

#define PATH_PROC 			"/proc/"
#define PATH_CPU_STATS		PATH_PROC "stat"
#define PATH_DISK_STATS 	PATH_PROC "diskstats"
#define PATH_MOUNT_STATS 	PATH_PROC "self/mountstats"
#define PATH_MEM_INFO 		PATH_PROC "meminfo"

enum program_states{
	ps_Undefined 	= 0b0000,
	ps_Ready 		= 0b0001,
	ps_Initialized 	= 0b0010,
	ps_Running 		= 0b0100,
	ps_Stopped		= 0b1000
}Program_State;

enum program_flags{
	pf_None 			= 0b0,
	pf_No_CLI_Output 	= 0b1
}Program_Flag;

enum initialization_states{
	is_None 				= 0b00000,
	is_Cpu 					= 0b00001,
	is_Ram 					= 0b00010,
	is_Disk_io 				= 0b00100,
	is_Network_Interface	= 0b01000,
	is_Filesystems 			= 0b10000
}Init_State;

typedef struct thread_container{
	pthread_t *thread;
	uint16_t id;
	void *parameter;
}thread_container_t;

struct disk_info{
	char *name;
	size_t read_bytes;
	size_t written_bytes;
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
	uint16_t type;
	size_t bandwith_mbps;
	size_t up_bps;
	size_t down_bps;
};

typedef struct network_interfaces{
	struct net_int_info *info;
	uint16_t count;
}net_ints_t;

// Globals
extern uint32_t Disk_Interval,
				NetInt_Interval;

extern enum program_flags			Program_Flag;
extern enum program_states 			Program_State;
extern enum initialization_states 	Init_State;


// Functions
void 		getCpuTimings			(uint32_t *cpu_total, uint32_t *cpu_idle, REQUIRE_WITH_SIZE(char *, cpu_identifier));
void * 		getCpuUsage				(void *thread_container);
uint64_t 	getFirstVarNumValue		(const char* path, REQUIRE_WITH_SIZE(const char*, variable), const uint16_t variable_column_no );
char * 		getSystemDisk			(char* os_partition_name, char* maj_no);
void *		getDiskReadWrite		(void *disk_info_ptr);
void 		getAllDisks				(disks_t *disks);
void 		getPhysicalFilesystems	(filesystems_t *filesystems);
void 		getNetworkInterfaces	(net_ints_t *netints);
void 		getNetworkIntUsage		(struct net_int_info *netint, uint32_t interval);
void *		timeLimit				(void *thread_container);


#endif