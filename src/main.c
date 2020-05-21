/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <glib.h>

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

	resource_thread_ty *disk_io_tcs,
						*net_int_tcs,
						cpu_tc,
						ram_tc,
						fss_tc;

	disks_ty disks;
	filesystems_ty filesystems;
	net_ints_ty netints;

	uint32_t	cpu_interval	= DEFAULT_CPU_INTERVAL,
				ram_interval	= DEFAULT_RAM_INTERVAL,
				disk_interval	= DEFAULT_DISK_INTERVAL,
				netint_interval = DEFAULT_NETINT_INTERVAL,
				filesys_interval= DEFAULT_FILESYS_INTERVAL;
	
	gfloat	cpu_alert_usage		= 200.0,
			ram_alert_usage		= 200.0,
			disk_alert_usage	= 200.0,
			netint_alert_usage 	= 200.0,
			filesys_alert_usage	= 200.0;

	extern char* optarg;
	int32_t opt;
#define _ARGS_ 	"ht:c:C:r:f:F:d:n:N:"
	while ((opt = getopt(argc, argv, _ARGS_)) != -1){

		switch (opt) {
			case 'h':

				g_fprintf(STD, "Usage: %s [-" _ARGS_ "] [value]\n", argv[0]);
				exit(EXIT_SUCCESS);

			break;
			case 't':
				{
					int64_t timelimit_ms = INT64_MIN;
					resource_thread_ty tlc;

					str_to_int64(optarg, &timelimit_ms);
					tlc.id = MAX_THREADS - 1;
					tlc.parameter = &timelimit_ms;

					pthread_create(&tlc.thread, NULL, timeLimit, &tlc);
				}
			break;
			case 'c':

				str_to_uint32(optarg, &cpu_interval);

			break;
			case 'C':
				
				str_to_float(optarg, &cpu_alert_usage);

			break;
			case 'r':

				str_to_uint32(optarg, &ram_interval);

			break;
			case 'f':

				str_to_uint32(optarg, &filesys_interval);

			break;
			case 'F':
				
				str_to_float(optarg, &filesys_alert_usage);

			break;
			case 'd':

				str_to_uint32(optarg, &disk_interval);

			break;
			case 'n':

				str_to_uint32(optarg, &netint_interval);

			break;
			case 'N':
				
				str_to_float(optarg, &netint_alert_usage);

			break;
			
			/*
			*	TODO (?) x for read from xml for alerts
			*	cpu 90% 5min
			*	ram 7gig 10min
			*/

			default:				
				g_fprintf(STD, "Usage: %s [-" _ARGS_ "] [value]\n", argv[0]);
				exit(EXIT_FAILURE);
		}//switch
	}

	getAllDisks(&disks); //Disks
	getNetworkInterfaces(&netints); //Network Interfaces

	if(disks.count > 1)
	{
		getSystemDisk(NULL, NULL); //System Disk
	}
	else if (disks.count == 0)
	{
		g_fprintf(STD, RED_BOLD("[ERROR]")" Could not get the disks!\n");
		exit(EXIT_FAILURE);
	}
	

/*
*	Allocate
*/

	disk_io_tcs = g_malloc(disks.count   * sizeof(resource_thread_ty));
	net_int_tcs = g_malloc(netints.count * sizeof(resource_thread_ty));

/*
*	Create
*/

	//Cpu
	cpu_tc.id = Last_Thread_Id++;
	cpu_tc.interval = cpu_interval;
	cpu_tc.alert_usage = cpu_alert_usage;
	cpu_tc.parameter = NULL;

	pthread_create(&cpu_tc.thread, NULL, getCpuUsage, &cpu_tc); // CPU
	//upC

	//Ram
	ram_tc.id = Last_Thread_Id++;
	ram_tc.interval = ram_interval;
	ram_tc.alert_usage = ram_alert_usage;
	ram_tc.parameter = NULL;

	pthread_create(&ram_tc.thread, NULL, getRamUsage, &ram_tc);
	//maR

	//Filesystems
	fss_tc.id = Last_Thread_Id++;
	fss_tc.interval = filesys_interval;
	fss_tc.alert_usage = filesys_alert_usage;
	fss_tc.parameter = &filesystems;

	pthread_create(&fss_tc.thread, NULL, getFilesystemsUsage, &fss_tc);
	//smetsyseliF

	//Disk
	for (uint16_t i = 0; i < disks.count; i++){ // Disks Reads/Writes
		(*(disk_io_tcs + i)).id = Last_Thread_Id++;
		(*(disk_io_tcs + i)).interval = disk_interval;
		(*(disk_io_tcs + i)).alert_usage = disk_alert_usage;
		(disk_io_tcs + i) -> parameter = disks.info + i;

		pthread_create( &(disk_io_tcs + i)->thread, NULL, getDiskUsage, disk_io_tcs + i );
	}
	//ksiD

	//Network
	for (uint16_t i = 0; i < netints.count; i++){ // Get Network Interface Infos
		(*(net_int_tcs + i)).id = Last_Thread_Id++;
		(*(net_int_tcs + i)).interval = netint_interval;
		(*(net_int_tcs + i)).alert_usage = netint_alert_usage;
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

	g_free(disk_io_tcs);
	g_free(net_int_tcs);
	Program_State = ps_Stopped;

#ifdef DEBUG_RUT
	clock_t _end = clock();
	fprintf(STD, "\nExecution time: %lf\n", (double)(_end - _begin) / CLOCKS_PER_SEC);
#endif
	return (EXIT_SUCCESS);
}

// $ cat /sys/block/<disk>/device/model ---> KBG30ZMV256G TOSHIBA