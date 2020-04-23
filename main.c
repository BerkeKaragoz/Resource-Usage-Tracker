/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include "resource_usage_tracker.h"
#include "berkelib/macros_.h"
#include "berkelib/utils_.h"

#define DEBUG_RUT

#ifdef DEBUG_RUT
#include <time.h>
#endif

// Globals
uint32_t 	Disk_Interval 	= DEFAULT_GLOBAL_INTERVAL,
			NetInt_Interval = DEFAULT_GLOBAL_INTERVAL;

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
		fprintf(STD, RED_BOLD("[ERROR] CPU Interval is lower than 0. It is set to %d.\n"), DEFAULT_GLOBAL_INTERVAL);
	}

	getCpuUsage(interval);

	return NULL;
}

void *call_getNetworkIntUsage(void* net_int_info_ptr){

	struct net_int_info *niip = (struct net_int_info *) net_int_info_ptr;
	
	getNetworkIntUsage(niip, NetInt_Interval);

	return NULL;
}

//todo: multithread safe str_split runcmd
int main (){
#ifdef DEBUG_RUT
	clock_t _begin = clock();
#endif
	fprintf(STD, "\n");

	pthread_t 	*disk_io_threads,
				*net_int_threads,
				cpu_thread;

	disks_t disks;
	filesystems_t filesystems;
	net_ints_t netints;

	uint32_t cpu_interval;

	//RAM
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("Active:"), 0);
	getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("MemTotal:"), 0);

	getAllDisks(&disks); //Disks
	getPhysicalFilesystems(&filesystems); //Filesystems
	getNetworkInterfaces(&netints); //Network Interfaces

	if(disks.count > 1)
	{
		getSystemDisk(NULL, NULL); //System Disk
	}
	else if (disks.count <= 0)
	{
		fprintf(STD, RED_BOLD("[ERROR]")" Could not get the disks!\n");
		exit(EXIT_FAILURE);
	}
	
	if (disks.count > MAX_THREADS)
	{
		fprintf(STD, RED_BOLD("[ERROR]")" Disk count exceeded maximum amount of threads!\n");
		exit(EXIT_FAILURE);
	}

/*
*	Allocate
*/

	disk_io_threads = malloc(disks.count * sizeof(pthread_t)); 
	net_int_threads = malloc(netints.count * sizeof(pthread_t)); 

/*
*	Create 
*/

	cpu_interval = DEFAULT_GLOBAL_INTERVAL;
	pthread_create(&cpu_thread, NULL, call_getCpuUsage, &cpu_interval); // CPU

	for (uint16_t i = 0; i < disks.count; i++){ // Disks Reads/Writes
		pthread_create(disk_io_threads + i, NULL, call_getDiskReadWrite, disks.info + i);
	}

	for (uint16_t i = 0; i < netints.count; i++){ // Get Network Interface Infos
		pthread_create(net_int_threads + i, NULL, call_getNetworkIntUsage, netints.info + i);
	}

/*
*	Join
*/

	for (uint16_t i = 0; i < disks.count; i++){ // Join Thread Disks
		pthread_join(*(disk_io_threads + i), NULL);
	}

	for (uint16_t i = 0; i < netints.count; i++){ // Join Thread Network Interfaces
		pthread_join(*(net_int_threads + i), NULL);
	}

	pthread_join(cpu_thread, NULL); // Join Thread CPU

/*
*	Clean Up
*/

	pthread_exit(NULL);
	free(disk_io_threads);
	free(net_int_threads);

#ifdef DEBUG_RUT
	clock_t _end = clock();
	fprintf(STD, "\nExecution time: %lf\n", (double)(_end - _begin) / CLOCKS_PER_SEC);
#endif
	return (EXIT_SUCCESS);
}

// $ cat /sys/block/<disk>/device/model ---> KBG30ZMV256G TOSHIBA