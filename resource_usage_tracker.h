/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#ifndef RESOURCE_USAGE_TRACKER_H
#define RESOURCE_USAGE_TRACKER_H

#include <stdint.h>
#include <stddef.h>

#include "berkelib/macros_.h"

#define DEFAULT_GLOBAL_INTERVAL 1000 // 1000 is consistent minimum for cpu on physical machines, use higher for virtuals
#define MAX_THREADS 64

#define PATH_PROC 			"/proc/"
#define PATH_CPU_STATS		PATH_PROC "stat"
#define PATH_DISK_STATS 	PATH_PROC "diskstats"
#define PATH_MOUNT_STATS 	PATH_PROC "self/mountstats"
#define PATH_MEM_INFO 		PATH_PROC "meminfo"

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
	uint16_t type;
	size_t bandwith_mbps;
	size_t up_bps;
	size_t down_bps;
};

typedef struct network_interfaces{
	struct net_int_info *info;
	uint16_t count;
}net_ints_t;

void 		getCpuTimings			(uint32_t *cpu_total, uint32_t *cpu_idle);
float 		getCpuUsage				(const uint32_t ms_interval);
uint64_t 	getFirstVarNumValue		(const char* path, REQUIRE_WITH_SIZE(const char*, variable), const uint16_t variable_column_no );
char * 		getSystemDisk			(char* os_partition_name, char* maj_no);
void 		getDiskReadWrite		(const uint32_t ms_interval, REQUIRE_WITH_SIZE(char *, disk_name), size_t *bread_sec_out, size_t *bwrite_sec_out);
void 		getAllDisks				(disks_t *disks);
void 		getPhysicalFilesystems	(filesystems_t *filesystems);
void 		getNetworkInterfaces	(net_ints_t *netints);
void 		getNetworkIntUsage		(struct net_int_info *netint, uint32_t interval);

#endif