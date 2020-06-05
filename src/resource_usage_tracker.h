/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#ifndef RESOURCE_USAGE_TRACKER_H
#define RESOURCE_USAGE_TRACKER_H

#include <stdint.h>
#include <stddef.h>
#include <glib/gtypes.h>

#include "berkelib/macros_.h"

#define DEBUG_RUT
#define MAX_THREADS 64
#define DEFAULT_GLOBAL_INTERVAL 1000
#define POLICY_FILE "xml/rut.xml"

#define DEFAULT_CPU_INTERVAL		DEFAULT_GLOBAL_INTERVAL
#define DEFAULT_RAM_INTERVAL		DEFAULT_GLOBAL_INTERVAL	* 2
#define DEFAULT_DISK_INTERVAL		DEFAULT_GLOBAL_INTERVAL	* 2
#define DEFAULT_NETINT_INTERVAL		DEFAULT_GLOBAL_INTERVAL * 2
#define DEFAULT_FILESYS_INTERVAL	DEFAULT_GLOBAL_INTERVAL * 10

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

typedef struct rut_config{
	uint32_t	timelimit_ms;

	uint32_t	cpu_interval,
				ram_interval,
				disk_interval,
				netint_interval,
				filesys_interval;
	
	gfloat		cpu_alert_usage,
				ram_alert_usage,
				disk_alert_usage,
				netint_alert_usage,
				filesys_alert_usage;
}rut_config_ty;

typedef struct resource_thread{
	pthread_t thread;
	uint16_t id;
	uint16_t output_line;
	uint32_t interval;
	gfloat alert_usage;
	void *parameter;
}resource_thread_ty;

struct disk_info{
	gchar *name;
	size_t read_cache;
	size_t write_cache;
	size_t read_bytes;
	size_t written_bytes;
};

typedef struct disks{
	struct resource_thread *threads;
	uint16_t count;
}disks_ty;

struct filesystem_info{
	gchar *partition;
	size_t block_size;
	size_t used;
	size_t available;
};

typedef struct filesystems{
	struct filesystem_info *info;
	uint16_t count;
}filesystems_ty;

struct net_int_info{
	gchar *name;
	uint16_t type;
	size_t bandwith_mbps;
	size_t up_bps;
	size_t down_bps;
};

typedef struct network_interfaces{
	struct net_int_info *info;
	uint16_t count;
}net_ints_ty;

typedef struct ram_info{
	size_t usage, capacity;
}ram_ty;

typedef struct cpu_info{
	uint32_t total, idle;
}cpu_ty;

// Globals
extern uint16_t Last_Thread_Id;

extern enum program_flags			Program_Flag;
extern enum program_states 			Program_State;
extern enum initialization_states 	Init_State;


// Functions
void		sendAlert				(resource_thread_ty *thread_container, gfloat usage);

void *		timeLimit				(void *thread_container);

void 		initCpu					(void *thread_container);
void 		initRam					(void *thread_container);
void 		initFilesystems			(void *thread_container);

void * 		getCpuUsage				(void *thread_container);
void *		getRamUsage				(void *thread_container);
void *		getDiskUsage			(void *thread_container);
void *		getNetworkIntUsage		(void *thread_container);
void *		getFilesystemsUsage		(void *thread_container);

void 		getCpuTimings			(uint32_t *cpu_total, uint32_t *cpu_idle, REQUIRE_WITH_SIZE(gchar *, cpu_identifier));
void 		getNetworkInterfaces	(net_ints_ty *netints);

int64_t 	getFirstVarNumValue		(const gchar* path, REQUIRE_WITH_SIZE(const gchar*, variable), const uint16_t variable_column_no );
gchar * 	getSystemDisk			(gchar* os_partition_name, gchar* maj_no);
void 		getAllDisks				(disks_ty *disks);
void 		getPhysicalFilesystems	(filesystems_ty *filesystems);


#endif