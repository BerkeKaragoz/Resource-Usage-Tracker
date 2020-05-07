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

#ifdef DEBUG_RUT
#include <time.h>
#endif


void *call_getNetworkIntUsage(void* net_int_info_ptr){

	struct net_int_info *niip = (struct net_int_info *) net_int_info_ptr;
	
	getNetworkIntUsage(niip, NetInt_Interval);

	return NULL;
}

int main (int argc, char * const argv[]){
#ifdef DEBUG_RUT
	clock_t _begin = clock();
#else
	CONSOLE_RESET();
#endif

	Program_State |= ps_Initialized;
	Program_State |= ps_Running;

	pthread_t 	*disk_io_threads,
				*net_int_threads,
				cpu_thread;

	disks_t disks;
	filesystems_t filesystems;
	net_ints_t netints;

	uint32_t cpu_interval;

	extern char* optarg;
	int32_t opt;
	#define _ARGS_ "t"
	while ((opt = getopt(argc, argv, _ARGS_":T")) != -1){

		switch (opt) {
			case 't':
				SOUT("s", "ARGSSSSSS");

				int64_t timelimit_ms = 3000;
				pthread_t timelimit_thread;
				thread_container_t tlc;
				
				timelimit_ms = atol(optarg); //TODO validation

				tlc.id = UINT16_MAX;
				tlc.thread = &timelimit_thread;
				tlc.parameter = &timelimit_ms;

				pthread_create(tlc.thread, NULL, timeLimit, &tlc);

			break;

			default:				
				fprintf(STD, "Usage: %s [-" _ARGS_ "] [value]\n", argv[0]);
				exit(EXIT_FAILURE);
		}//switch
	}

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
	

/*
*	Allocate
*/

	disk_io_threads = malloc(disks.count * sizeof(pthread_t)); 
	net_int_threads = malloc(netints.count * sizeof(pthread_t)); 

/*
*	Create
*/

	cpu_interval = 1234;
	thread_container_t Cpu_Thread;
	Cpu_Thread.id = 0;
	Cpu_Thread.thread = &cpu_thread;
	Cpu_Thread.parameter = &cpu_interval;


	pthread_create(Cpu_Thread.thread, NULL, getCpuUsage, &Cpu_Thread); // CPU

	for (uint16_t i = 0; i < disks.count; i++){ // Disks Reads/Writes
		pthread_create(disk_io_threads + i, NULL, getDiskReadWrite, disks.info + i);
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

	pthread_join(Cpu_Thread.thread, NULL); // Join Thread CPU

/*
*	Clean Up
*/

	pthread_exit(NULL);
	free(disk_io_threads);
	free(net_int_threads);

	Program_State = ps_Stopped;

#ifdef DEBUG_RUT
	clock_t _end = clock();
	fprintf(STD, "\nExecution time: %lf\n", (double)(_end - _begin) / CLOCKS_PER_SEC);
#endif
	return (EXIT_SUCCESS);
}

// $ cat /sys/block/<disk>/device/model ---> KBG30ZMV256G TOSHIBA