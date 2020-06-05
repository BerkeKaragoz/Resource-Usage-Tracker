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
#include "xmlparser.h"

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

	rut_config_ty config;

	cpu_ty cpu;
	ram_ty ram;
	disks_ty disks;
	filesystems_ty filesystems;
	net_ints_ty netints;

	resource_thread_ty *disk_io_tcs,
						*net_int_tcs,
						cpu_tc,
						ram_tc,
						fss_tc;

	//Config
	config.timelimit_ms = 0;

	config.cpu_interval		= DEFAULT_CPU_INTERVAL,
	config.ram_interval		= DEFAULT_RAM_INTERVAL,
	config.disk_interval	= DEFAULT_DISK_INTERVAL,
	config.netint_interval 	= DEFAULT_NETINT_INTERVAL,
	config.filesys_interval	= DEFAULT_FILESYS_INTERVAL;
	
	config.cpu_alert_usage		= 200.0,
	config.ram_alert_usage		= 200.0,
	config.disk_alert_usage		= 200.0,
	config.netint_alert_usage 	= 200.0,
	config.filesys_alert_usage	= 200.0;
	//gifnoC

	extern char* optarg;
	int32_t opt;

	while ((opt = getopt(argc, argv, "hqt:c:C:r:R:f:F:d:D:n:N:x")) != -1){

		switch (opt) {
			case 'x':

				readXmlTree(&config, POLICY_FILE);

			break;
			case 'q':

				Program_Flag |= pf_No_CLI_Output;

			break;
			case 't':

				str_to_uint32(optarg, &config.timelimit_ms);

			break;
			case 'c':

				str_to_uint32(optarg, &config.cpu_interval);

			break;
			case 'C':
				
				str_to_float(optarg, &config.cpu_alert_usage);

			break;
			case 'r':

				str_to_uint32(optarg, &config.ram_interval);

			break;
			case 'R':
				
				str_to_float(optarg, &config.ram_alert_usage);

			break;
			case 'f':

				str_to_uint32(optarg, &config.filesys_interval);

			break;
			case 'F':
				
				str_to_float(optarg, &config.filesys_alert_usage);

			break;
			case 'd':

				str_to_uint32(optarg, &config.disk_interval);

			break;
			case 'D':
				
				str_to_float(optarg, &config.disk_alert_usage);//TODO

			break;
			case 'n':

				str_to_uint32(optarg, &config.netint_interval);

			break;
			case 'N':
				
				str_to_float(optarg, &config.netint_alert_usage);

			break;

			case 'h':
			default:

				g_fprintf(STD, "Usage: %s [option] [value]\n" \
					"  -t Sets the timelimit of the program in ms. Program exits after that time.\n" \
					"  -q Quiet Mode\n"
					"  -c Sets the CPU usage checking interval in ms\n\t-C sets the alert limit in usage percentage\n" \
					"  -r Sets the RAM usage checking interval in ms\n\t-C sets the alert limit in usage percentage\n" \
					"  -d Sets the Disks usage checking interval in ms\n\t-C sets the alert limit in usage percentage\n" \
					"  -f Sets the Filesystems usage checking interval in ms\n\t-C sets the alert limit in usage percentage\n" \
					"  -n Sets the Network Interfaces usage checking interval in ms\n\t-C sets the alert limit in usage percentage\n" \
				, argv[0]);
				exit(EXIT_SUCCESS);

		}//switch
	}

	getAllDisks(&disks); //Disks
	getNetworkInterfaces(&netints); //Network Interfaces


	if(disks.count > 1)
	{
		
		getSystemDisk(NULL, NULL); //System Disk

	} else if (disks.count == 0) {

		g_fprintf(STD, RED_BOLD("[ERROR]")" Could not get the disks!\n");
		exit(EXIT_FAILURE);

	}

	if (config.timelimit_ms > 0){
		resource_thread_ty tlc;

		tlc.id = MAX_THREADS - 1;
		tlc.parameter = &config.timelimit_ms;

		pthread_create(&tlc.thread, NULL, timeLimit, &tlc);
	}
	

/*
*	Allocate
*/

	net_int_tcs = g_malloc(netints.count * sizeof(resource_thread_ty));

/*
*	Create
*/

	//Cpu
	cpu_tc.id = Last_Thread_Id++;
	cpu_tc.interval = config.cpu_interval;
	cpu_tc.alert_usage = config.cpu_alert_usage;
	cpu_tc.parameter = &cpu;

	pthread_create(&cpu_tc.thread, NULL, getCpuUsage, &cpu_tc); // CPU
	//upC

	//Ram
	ram_tc.id = Last_Thread_Id++;
	ram_tc.interval = config.ram_interval;
	ram_tc.alert_usage = config.ram_alert_usage;
	ram_tc.parameter = &ram;

	pthread_create(&ram_tc.thread, NULL, getRamUsage, &ram_tc);
	//maR

	//Filesystems
	fss_tc.id = Last_Thread_Id++;
	fss_tc.interval = config.filesys_interval;
	fss_tc.alert_usage = config.filesys_alert_usage;
	fss_tc.parameter = &filesystems;

	pthread_create(&fss_tc.thread, NULL, getFilesystemsUsage, &fss_tc);
	//smetsyseliF

	//Disk
	for (uint16_t i = 0; i < disks.count; i++){ // Disks Reads/Writes

		(disks.threads + i) -> id = Last_Thread_Id++;

		(disks.threads + i) -> interval = config.disk_interval;

		(disks.threads + i) -> alert_usage = config.disk_alert_usage;


		pthread_create( &(disks.threads + i)->thread, NULL, getDiskUsage, disks.threads + i );

	}
	//ksiD

	//Network
	for (uint16_t i = 0; i < netints.count; i++){ // Get Network Interface Infos
		(*(net_int_tcs + i)).id = Last_Thread_Id++;
		(*(net_int_tcs + i)).interval = config.netint_interval;
		(*(net_int_tcs + i)).alert_usage = config.netint_alert_usage;
		(net_int_tcs + i) -> parameter = netints.info + i;

		pthread_create( &(net_int_tcs + i)->thread, NULL, getNetworkIntUsage, net_int_tcs + i);
	}
	//krowteN

/*
*	Join
*/
/*
	for (uint16_t i = 0; i < disks.count; i++){ // Join Thread Disks
		pthread_join( (disk_io_tcs + i)->thread, NULL );
	}
*/
	for (uint16_t i = 0; i < netints.count; i++){ // Join Thread Network Interfaces
		pthread_join( (net_int_tcs + i)->thread, NULL );
	}

	pthread_join(cpu_tc.thread, NULL); // Join Thread CPU

/*
*	Clean Up
*/

//	g_free(disk_io_tcs);
	g_free(net_int_tcs);
	Program_State = ps_Stopped;

#ifdef DEBUG_RUT
	clock_t _end = clock();
	fprintf(STD, "\nExecution time: %lf\n", (double)(_end - _begin) / CLOCKS_PER_SEC);
#endif
	return (EXIT_SUCCESS);
}