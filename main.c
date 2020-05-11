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

uint16_t Last_Thread_Id = 1;


int main (int argc, char * const argv[]){
#ifdef DEBUG_RUT
	clock_t _begin = clock();
#else
	CONSOLE_RESET();
#endif

	Program_State |= ps_Initialized;
	Program_State |= ps_Running;

	thread_container_t  *disk_io_tcs,
						*net_int_tcs,
						cpu_tc,
						ram_tc;

	disks_t disks;
	filesystems_t filesystems;
	net_ints_t netints;

	extern char* optarg;
	int32_t opt;
	#define _ARGS_ "t"
	while ((opt = getopt(argc, argv, _ARGS_":T")) != -1){

		switch (opt) {
			case 't':
				{
					int64_t timelimit_ms = INT64_MIN;
					thread_container_t tlc;

					timelimit_ms = atol(optarg); //TODO validation

					tlc.id = UINT16_MAX;
					tlc.parameter = &timelimit_ms;

					pthread_create(&tlc.thread, NULL, timeLimit, &tlc);
				}
			break;

			default:				
				fprintf(STD, "Usage: %s [-" _ARGS_ "] [value]\n", argv[0]);
				exit(EXIT_FAILURE);
		}//switch
	}

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

	disk_io_tcs = malloc(disks.count   * sizeof(thread_container_t));
	net_int_tcs = malloc(netints.count * sizeof(thread_container_t));

/*
*	Create
*/

	//Cpu
	cpu_tc.id = Last_Thread_Id++;
	cpu_tc.parameter = NULL;

	pthread_create(&cpu_tc.thread, NULL, getCpuUsage, &cpu_tc); // CPU
	//upC

	//Ram
	ram_tc.id = Last_Thread_Id++;
	ram_tc.parameter = NULL;

	pthread_create(&ram_tc.thread, NULL, getRamUsage, &ram_tc);
	//maR

	//Disk
	for (uint16_t i = 0; i < disks.count; i++){ // Disks Reads/Writes
		(*(disk_io_tcs + i)).id = Last_Thread_Id++;
		(disk_io_tcs + i) -> parameter = disks.info + i;

		pthread_create( &(disk_io_tcs + i)->thread, NULL, getDiskReadWrite, disk_io_tcs + i );
	}
	//ksiD

	//Network
	for (uint16_t i = 0; i < netints.count; i++){ // Get Network Interface Infos
		(*(net_int_tcs + i)).id = Last_Thread_Id++;
		(net_int_tcs + i) -> parameter = netints.info + i;

		pthread_create( &(net_int_tcs + i)->thread, NULL, getNetworkIntUsage, net_int_tcs + i);
	}
	//krowteN

/*
*	Join
*/

	for (uint16_t i = 0; i < disks.count; i++){ // Join Thread Disks
		pthread_join( (disk_io_tcs + i)->thread, NULL );
	}

	for (uint16_t i = 0; i < netints.count; i++){ // Join Thread Network Interfaces
		pthread_join( (net_int_tcs + i)->thread, NULL );
	}

	pthread_join(cpu_tc.thread, NULL); // Join Thread CPU

/*
*	Clean Up
*/

	pthread_exit(NULL);
	free(disk_io_tcs);
	free(net_int_tcs);
	Program_State = ps_Stopped;

#ifdef DEBUG_RUT
	clock_t _end = clock();
	fprintf(STD, "\nExecution time: %lf\n", (double)(_end - _begin) / CLOCKS_PER_SEC);
#endif
	return (EXIT_SUCCESS);
}

// $ cat /sys/block/<disk>/device/model ---> KBG30ZMV256G TOSHIBA