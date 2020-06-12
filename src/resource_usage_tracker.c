/*
 *  	E. Berke Karagöz
 *      e.berkekaragoz@gmail.com
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <linux/if_arp.h>
#include <glib.h>

#include "berkelib/macros_.h"
#include "berkelib/utils_.h"
#include "resource_usage_tracker.h"

/*
*	Globals
*/
#ifdef DEBUG_RUT
enum program_flags			Program_Flag	= pf_No_CLI_Output;
#else
enum program_flags			Program_Flag	= pf_None;
#endif
enum program_states 		Program_State 	= ps_Ready;
enum initialization_states 	Init_State 		= is_None;


pthread_mutex_t Cpu_Mutex 		= PTHREAD_MUTEX_INITIALIZER,
				Ram_Mutex		= PTHREAD_MUTEX_INITIALIZER,
				Disk_io_Mutex 	= PTHREAD_MUTEX_INITIALIZER,
				Net_int_Mutex	= PTHREAD_MUTEX_INITIALIZER,
				File_sys_Mutex	= PTHREAD_MUTEX_INITIALIZER;


/*
*	Functions
*/

void sendAlert (resource_thread_ty *thread_container, gfloat usage){

	if ( !(Program_Flag & pf_No_CLI_Output) ){

		CONSOLE_GOTO(0, thread_container->output_line);
		g_fprintf(STD, PINK_BOLD("█"));

		CONSOLE_GOTO(0, Last_Thread_Id + 1);

		g_fprintf(STD, CONSOLE_ERASE_LINE PINK_BOLD("[ALERT]") " Resource (" WHITE_BOLD("%" PRIu16) ") usage (" WHITE_BOLD("%2.2f%%") ") is over " WHITE_BOLD("%2.2f%%") " in last " WHITE_BOLD("%" PRIu16 "ms") "!",
			thread_container->id, usage, thread_container->alert_usage, thread_container->interval
		);
	}

}

/*
*
*/
void *timeLimit (void *thread_container){

	resource_thread_ty tc = *((resource_thread_ty *) thread_container);

	uint32_t timelimit = DEFAULT_GLOBAL_INTERVAL * 3;

	if ( *((int64_t *) tc.parameter) >= 0 ){

		timelimit = *((uint32_t *) tc.parameter);

	} else {

		g_fprintf(STD, RED_BOLD("[ERROR] Timelimit (%" PRId64 ") is lower than 1. It is set to %" PRIu32 ".\n"), (*((int64_t *) tc.parameter)), DEFAULT_GLOBAL_INTERVAL*3);

	}

	sleep_ms(timelimit);

	g_fprintf(STD, CONSOLE_ERASE_LINE "Timelimit is over.\n" CONSOLE_ERASE_LINE "Success!\n");
	Program_State = ps_Stopped;

	exit(EXIT_SUCCESS);
}

/*
*
*/

void snapAll(
		rut_config_ty *config,
		uint16_t *last_thread_id,
		resource_thread_ty *cpu_th,
		resource_thread_ty *ram_th,
		resource_thread_ty *fss_th,
		resources_ty *disks,
		resources_ty *netints
	){
	// TODO

	if (cpu_th != NULL)
		snapCpu(cpu_th);

	if (ram_th != NULL)
		snapRam(ram_th);

	if (fss_th != NULL)
		snapFss(fss_th);
		
	if (disks != NULL)
		snapAllDisks(disks, config, last_thread_id);
	
	if (netints != NULL)
		snapAllNetints(netints, config, last_thread_id);
	
}

void snapAllDisks(resources_ty *disks, rut_config_ty *config, uint16_t *last_thread_id){

	getAllDisks(disks);


	uint16_t i;

	for(i = 0; i < disks->count; i++){

		(disks->threads + i) -> id = (*last_thread_id)++;

		(disks->threads + i) -> interval = config->disk_interval;

		(disks->threads + i) -> alert_usage = config->disk_alert_usage;


		pthread_create( &(disks->threads + i)->thread, NULL, snapDisk, disks->threads + i );

	}

	for (i = 0; i < disks->count; i++){ // Join Thread Disks

		pthread_join( (disks->threads + i)->thread, NULL );

	}

	Init_State |= is_Disk_io;

}

void snapAllNetints(resources_ty *netints, rut_config_ty *config, uint16_t *last_thread_id){

	getNetworkInterfaces(netints);


	uint16_t i;

	for(i = 0; i < netints->count; i++){

		(netints->threads + i) -> id = (*last_thread_id)++;

		(netints->threads + i) -> interval = config->netint_interval;

		(netints->threads + i) -> alert_usage = config->netint_alert_usage;

		pthread_create( &(netints->threads + i)->thread, NULL, snapNetint, netints->threads + i );

	}

	for (i = 0; i < netints->count; i++){ // Join Thread Network Ints

		pthread_join( (netints->threads + i)->thread, NULL );

	}

	Init_State |= is_Network_Interface;
}

/*
*
*/

void *snapCpu(void *thread_container){
/*
*	Test The Parameter
*/

	pthread_mutex_lock(&Cpu_Mutex);

	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	if ( tc->interval < 10 ){//todo if 0 do not track

		tc->interval = DEFAULT_GLOBAL_INTERVAL;
		g_fprintf(STD, RED_BOLD("[ERROR] CPU Interval is lower than 10ms. It is set to %" PRIu32 ".\n"), DEFAULT_GLOBAL_INTERVAL);

	}

	cpu_ty *c = (cpu_ty *) tc->parameter;

/*
*	Initialize CPU Timings
*/

	c->total = 0;
	c->idle = 0;

	// Initial Output
	if ( !(Program_Flag & pf_No_CLI_Output) ){

		tc->output_line = tc->id;
		CONSOLE_GOTO(0, tc->output_line);
		g_fprintf(STD, CONSOLE_ERASE_LINE " CPU Usage: \tWaiting for %" PRIu32 "ms...\n", tc->interval);
		CONSOLE_GOTO(0, Last_Thread_Id + 1);
		g_fprintf(STD, CONSOLE_ERASE_LINE);
		fflush(STD);

	}//

	tc->cache_time = getCpuTimings(&c->total, &c->idle, PASS_WITH_SIZEOF("cpu"));

	Init_State |= is_Cpu;


	pthread_mutex_unlock(&Cpu_Mutex);
}

void *snapRam(void *thread_container){

	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	ram_ty *r = (ram_ty *) tc->parameter;


	r->capacity = getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("MemTotal:"	), 0);
	r->usage 	= getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("Active:"		), 0);

	tc->cache_time = g_get_monotonic_time();


	Init_State |= is_Ram;
}

void *snapDisk(void *thread_container){
/*
*	Parameter conversion
*/

	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	struct disk_info *dip = (struct disk_info *) tc->parameter;

/*
*	Defining Variables
*/

	gchar *input_cmd = (gchar *)g_malloc(
		sizeof(dip->name) * sizeof(gchar) + sizeof("awk '$3 == \"\" {print $6\"\\t\"$10}' /proc/diskstats")
	);

	g_stpcpy(input_cmd, "awk '$3 == \"");
	strcat(input_cmd, dip->name);
	strcat(input_cmd, "\" {print $6\"\\t\"$10}' /proc/diskstats");

	size_t temp_size = 0;
	gchar **read_write = ( gchar ** ) g_malloc ( sizeof(gchar *) * 2 );

/*
*	Initialize I/O Values
*/
	
	// Initial Output
	if ( !(Program_Flag & pf_No_CLI_Output) ){

		tc->output_line = tc->id;
		CONSOLE_GOTO(0, tc->output_line);
		g_fprintf(STD, CONSOLE_ERASE_LINE " Disk: %-10s\tWaiting for %" PRIu32 "ms...\n", dip->name, tc->interval);	
		CONSOLE_GOTO(0, Last_Thread_Id + 1);
		g_fprintf(STD, CONSOLE_ERASE_LINE);
		fflush(STD);

	}//

	gchar *command_output = run_command(input_cmd);  //hold output for percise timing

	tc->cache_time = g_get_monotonic_time();

	str_split(&read_write, command_output, '\t', &temp_size);

	str_to_uint64(read_write[0], &dip->read_cache);
	str_to_uint64(read_write[1], &dip->write_cache);


	free(read_write);
	free(input_cmd);
	command_output = NULL;

}

void *snapNetint(void *thread_container){
	
	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	struct net_int_info *nii = (struct net_int_info *) tc->parameter;

/*
*	Defining Variables
*/

	gchar *input_cmd = (gchar *)g_malloc(
		sizeof(nii->name) * sizeof(gchar) + sizeof("tail -n +3 /proc/net/dev | grep | awk '{print $2\" \"$10}'")
	);

	g_stpcpy(input_cmd, "tail -n +3 /proc/net/dev | grep ");
	strcat(input_cmd, nii->name);
	strcat(input_cmd, " | awk '{print $2\" \"$10}'");

	size_t temp_size = 0;
	gchar **down_up = ( gchar ** ) g_malloc ( sizeof(gchar *) * 2);

/*
*	Initialize Down/Up Values
*/

	// Initial Output
	if ( !(Program_Flag & pf_No_CLI_Output) ){

		tc->output_line = tc->id;
		CONSOLE_GOTO(0, tc->output_line);
		g_fprintf(STD, CONSOLE_ERASE_LINE " Network: %-7s\tWaiting for %" PRIu32 "ms...\n", nii->name, tc->interval);	
		CONSOLE_GOTO(0, Last_Thread_Id + 1);
		g_fprintf(STD, CONSOLE_ERASE_LINE);
		fflush(STD);

	}//
	

	gchar *command_output = run_command(input_cmd); //hold output for percise timing
	
	tc->cache_time = g_get_monotonic_time();

	str_split(&down_up, command_output, ' ', &temp_size);

	str_to_uint64(down_up[0], &nii->down_cache);
	str_to_uint64(down_up[1], &nii->up_cache);


	free(down_up);
	free(input_cmd);
	command_output = NULL;
}

void *snapFss(void *thread_container){

	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	filesystems_ty *fss = (filesystems_ty *) tc->parameter;


	pthread_mutex_lock(&File_sys_Mutex);

	tc->cache_time = getPhysicalFilesystems(fss);

	// Output
	if ( !(Program_Flag & pf_No_CLI_Output) ){

		pthread_mutex_lock(&File_sys_Mutex);

		CONSOLE_GOTO(0, Last_Thread_Id + 2);
		g_fprintf(STD, "Filesystems:");

		for(uint32_t i = 0; i < fss->count; i++){
			
			size_t total_size = ( (fss->info + i)->used + (fss->info + i)->available);
			
			gfloat percentage_usage = (fss->info + i)->used / (gfloat) total_size * 100.0;

			tc->output_line = Last_Thread_Id + 3 + i;
			CONSOLE_GOTO(0, tc->output_line);
			g_fprintf(STD, CONSOLE_ERASE_LINE " %s:\t%s / %s \t %2.2f%% \n", (fss->info + i)->partition, bytes_to_str((fss->info + i)->used), bytes_to_str(total_size), percentage_usage);

			if (percentage_usage >= tc->alert_usage){
				sendAlert(tc, percentage_usage);
			}

		}

		CONSOLE_GOTO(0, Last_Thread_Id + 1);
		g_fprintf(STD, CONSOLE_ERASE_LINE);

		fflush(STD);

		pthread_mutex_unlock(&File_sys_Mutex);

	}

	Init_State |= is_Filesystems;

	pthread_mutex_unlock(&File_sys_Mutex);

}

/*
*
*/

// 1000 ms is min. stable
// cat <(grep cpu /proc/stat) <(sleep 0.1 && grep cpu /proc/stat) | awk -v RS="" '{printf "%.1f", ($13-$2+$15-$4)*100/($13-$2+$15-$4+$16-$5)}
void *trackCpuUsage(void *thread_container){

	snapCpu(thread_container);


	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	cpu_ty *c = (cpu_ty *) tc->parameter;


	sleep_ms(tc->interval);

/*
*	Get CPU Usage Till The Program Stops
*/

	if( Init_State & is_Cpu ){


		uint32_t prev_total	= 0, 
				 prev_idle	= 0;
		gfloat 	 usage 		= 0.0;

		while( Program_State & ps_Running ){

			pthread_mutex_lock(&Cpu_Mutex);

			prev_idle 	= c->idle;
			prev_total 	= c->total;

			tc->cache_time = getCpuTimings(&c->total, &c->idle, PASS_WITH_SIZEOF("cpu"));
		
			usage = 100.0 * ( 1.0 - (gfloat)(c->idle - prev_idle) / (gfloat)(c->total - prev_total) );
			
			// Check Usage if it is corrupted
			if ( isnan(usage) ){

				usage = 0.0;		
				g_fprintf(STD, YELLOW_BOLD("[Warning]") " CPU usage is NaN, CPU's total values are unchanged.\n");

			}

			// Output
			if ( !(Program_Flag & pf_No_CLI_Output) ){

				CONSOLE_GOTO(0, tc->id);
				g_fprintf(STD, CONSOLE_ERASE_LINE " CPU Usage: %7.2f%%\n", usage);
				CONSOLE_GOTO(0, Last_Thread_Id + 1);
				g_fprintf(STD, CONSOLE_ERASE_LINE);
				fflush(STD);

			}

			if (usage >= tc->alert_usage){

				sendAlert(tc, usage);

			}

#ifdef DEBUG_RUT
			if (usage >= tc->alert_usage){
				g_fprintf(STD, PINK_BOLD("[ALERT]") " CPU usage (%2.2f%%) is over %2.2f%% in last %" PRIu32 "ms!\n", usage, tc->alert_usage, tc->interval);
			}

			g_fprintf(STD,
				CYAN_BOLD(" --- trackCpuUsage() --- Thread: %d\n")	\
					PR_VAR("f", USAGE)				\
					PR_VAR("f", Alert_Usage)		\
					PR_VAR(PRIu32, Cpu_Interval)	\
					PR_VAR(PRId32, delta-idle)		\
					PR_VAR(PRId32, delta-total)		\
					PR_VAR(PRId64, cache-time)		\
				CYAN_BOLD(" ---\n") 				\
				, tc->id, usage, tc->alert_usage, tc->interval, c->idle - prev_idle, c->total - prev_total, tc->cache_time
			);
#endif
			// tuptuO

			pthread_mutex_unlock(&Cpu_Mutex);

			sleep_ms(tc->interval);

		}

	} else {

		g_fprintf(STD, RED_BOLD("[ERROR]") " CPU Timings are not initialized.\n");
		return NULL;

	}

	return NULL;
}

void *trackRamUsage(void *thread_container){

	snapRam(thread_container);

	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	ram_ty *r = (ram_ty *) tc->parameter;

	sleep_ms(tc->interval);

/*
*	Get RAM Usage Till The Program Stops
*/

	if( Init_State & is_Ram ){

		while( Program_State & ps_Running ){

			r->usage = getFirstVarNumValue(PATH_MEM_INFO, PASS_WITH_SIZEOF("Active:"), 0);

			tc->cache_time = g_get_monotonic_time();

			
			pthread_mutex_lock(&Ram_Mutex);

			gfloat percentage_usage = r->usage * 100.0 / (gfloat) r->capacity;

			// Output
			if ( !(Program_Flag & pf_No_CLI_Output) ){

				tc->output_line = tc->id;
				CONSOLE_GOTO(0, tc->output_line);
				g_fprintf(STD, CONSOLE_ERASE_LINE " RAM:\t\t%s / %s\n", bytes_to_str(r->usage), bytes_to_str(r->capacity) );	
				CONSOLE_GOTO(0, Last_Thread_Id + 1);
				g_fprintf(STD, CONSOLE_ERASE_LINE);
				fflush(STD);
				
			}

			if (percentage_usage >= tc->alert_usage){

				sendAlert(tc, percentage_usage);

			}

#ifdef DEBUG_RUT
		if (percentage_usage >= tc->alert_usage){
			g_fprintf(STD, PINK_BOLD("[ALERT]") " RAM usage (%2.2f%%) is over %2.2f%% in last %" PRIu32 "ms!\n", percentage_usage, tc->alert_usage, tc->interval);
		}

		g_fprintf(STD,
			CYAN_BOLD(" --- trackRamUsage() --- Thread: %d\n") \
				PR_VAR(PRIu32, Ram_Interval)	\
				PR_VAR("f", percentage_usage)	\
				PR_VAR("f", alert_usage)		\
				PR_VAR("zu", usage)				\
				PR_VAR("zu", capacity)			\
				PR_VAR(PRId64, cache-time)		\
			CYAN_BOLD(" ---\n") 				\
			, tc->id, tc->interval, percentage_usage, tc->alert_usage, r->usage, r->capacity, tc->cache_time
		);
#endif
			// tuptuO

			pthread_mutex_unlock(&Ram_Mutex);

			sleep_ms(tc->interval);

		}

	} else {

		g_fprintf(STD, RED_BOLD("[ERROR]") " RAM values are not initialized.\n");
		return NULL;

	}

	return NULL;
}

// $ awk '$3 == "<DISK_NAME>" {print $6"\t"$10}' /proc/diskstats
void *trackDiskUsage(void *thread_container){

	snapDisk(thread_container);


	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	struct disk_info *dip = (struct disk_info *) tc->parameter;


	sleep_ms(tc->interval);

/*
*	Get Disk I/O Till The Program Stops
*/

	if( Init_State & is_Disk_io ){


		size_t temp_size = 0;

		gchar **read_write 	= ( gchar ** ) g_malloc ( sizeof(gchar *) * 2 );

		gchar *input_cmd = (gchar *)g_malloc(
			sizeof(dip->name) * sizeof(gchar) + sizeof("awk '$3 == \"\" {print $6\"\\t\"$10}' /proc/diskstats")
		);

		g_stpcpy(input_cmd, "awk '$3 == \"");
		strcat(input_cmd, dip->name);
		strcat(input_cmd, "\" {print $6\"\\t\"$10}' /proc/diskstats");


		while( Program_State & ps_Running ){

			pthread_mutex_lock(&Disk_io_Mutex);
		
			gchar* command_output = run_command(input_cmd); // hold output for precise timing

			tc->cache_time = g_get_monotonic_time();

			str_split(&read_write, command_output, '\t', &temp_size);

			command_output = NULL;


			dip -> read_bytes  	 = atoll(read_write[0]) - dip -> read_cache;
			dip -> written_bytes = atoll(read_write[1]) - dip -> write_cache;

			str_to_uint64(read_write[0], &dip->read_cache);
			str_to_uint64(read_write[1], &dip->write_cache);

			//TODO alert

			// Output
			if ( !(Program_Flag & pf_No_CLI_Output) ){

				CONSOLE_GOTO(0, tc->id);
				g_fprintf(STD, CONSOLE_ERASE_LINE " Disk: %-7s\tRead: %7s /%" PRIu32 "ms\tWrite: %7s /%" PRIu32 "ms\n", dip->name, bytes_to_str(dip->read_bytes), tc->interval, bytes_to_str(dip->written_bytes), tc->interval);	
				CONSOLE_GOTO(0, Last_Thread_Id + 1);
				g_fprintf(STD, CONSOLE_ERASE_LINE);
				fflush(STD);

			}

#ifdef DEBUG_RUT
	g_fprintf(STD,
		CYAN_BOLD(" --- trackDiskUsage(")"%s"CYAN_BOLD(") --- Thread: %d\n") \
			PR_VAR("d", Disk_Interval)		\
			PR_VAR("zu", read_cache)		\
			PR_VAR("zu", write_cache)		\
			PR_VAR("s", read_write[0])		\
			PR_VAR("s", read_write[1])		\
			PR_VAR("d", dip->read_bytes)	\
			PR_VAR("d", dip->written_bytes)	\
			PR_VAR(PRId64, cache-time)		\
		CYAN_BOLD(" ---\n") 				\
		, dip->name, tc->id, tc->interval, dip->read_cache, dip->write_cache, read_write[0], read_write[1], dip->read_bytes, dip->written_bytes, tc->cache_time
	);
#endif
			// tuptuO

			pthread_mutex_unlock(&Disk_io_Mutex);

			sleep_ms(tc->interval);

		}

		g_free(input_cmd);
		g_free(read_write);

	} else {

		g_fprintf(STD, RED_BOLD("[ERROR]") " Disk I/O values are not initialized.\n");
		return NULL;

	}

	return NULL;
}

/*
Get Bandwith
cat /sys/class/net/eth0/speed

Get Network Usage
awk '{if(l1){print $2-l1,$10-l2} else{l1=$2; l2=$10;}}' \
<(grep NET_INT_NAME /proc/net/dev) <(sleep 1; grep NET_INT_NAME /proc/net/dev)
*/
//tail -n +3 /proc/net/dev | grep NET_INT_NAME | column -t
void *trackNetintUsage(void *thread_container){

	snapNetint(thread_container);

	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	struct net_int_info *nii = (struct net_int_info *) tc->parameter;


	sleep_ms(tc->interval);

/*
*	Get Disk I/O Till The Program Stops
*/

	if( Init_State & is_Network_Interface ){


		size_t temp_size;
	
		str_ptrlen(&temp_size, nii->name);

		gchar *input_cmd = (gchar *)g_malloc(
			temp_size * sizeof(gchar) + sizeof("tail -n +3 /proc/net/dev | grep | awk '{print $2\" \"$10}'")
		);

		g_stpcpy(input_cmd, "tail -n +3 /proc/net/dev | grep ");
		strcat(input_cmd, nii->name);
		strcat(input_cmd, " | awk '{print $2\" \"$10}'");

		gchar **down_up = (gchar **)g_malloc(sizeof(gchar *) * 2);
		temp_size = 0;


		while( Program_State & ps_Running ){
			
			pthread_mutex_lock(&Net_int_Mutex);

			gchar* command_output = run_command(input_cmd); // hold output for precise timing

			tc->cache_time = g_get_monotonic_time();

			str_split(&down_up, command_output, ' ', &temp_size);

			command_output = NULL;


			nii -> down_bps = atoll(down_up[0]) - nii -> down_cache; // AD - BD
			nii -> up_bps 	= atoll(down_up[1]) - nii -> up_cache; // AU - BU

			str_to_uint64(down_up[0], &nii -> down_cache);
			str_to_uint64(down_up[1], &nii -> up_cache);
			
			gfloat percentage_usage = (nii->down_bps + nii->up_bps) / (gfloat) (nii->bandwith_mbps * MEGABYTE / 8) * 100.0;

			// Output
			if ( !(Program_Flag & pf_No_CLI_Output) ){

				tc->output_line = tc->id;
				CONSOLE_GOTO(0, tc->output_line);
				g_fprintf(STD, CONSOLE_ERASE_LINE " Network: %-10s\tDown: %7s /%" PRIu32 "ms\tUp: %7s /%" PRIu32 "ms  %2.4f%%\n", nii->name, bytes_to_str(nii->down_bps), tc->interval, bytes_to_str(nii->up_bps), tc->interval, percentage_usage);	
				CONSOLE_GOTO(0, Last_Thread_Id + 1);
				g_fprintf(STD, CONSOLE_ERASE_LINE);
				fflush(STD);
			}

			if (percentage_usage >= tc->alert_usage	){
				sendAlert(tc, percentage_usage);
			}

#ifdef DEBUG_RUT

	if (percentage_usage >= tc->alert_usage){
		g_fprintf(STD, PINK_BOLD("[ALERT]") " Network usage (%2.5f%%) is over %2.5f%% in last %" PRIu32 "ms!\n", percentage_usage, tc->alert_usage, tc->interval);
	}

	g_fprintf(STD,
		CYAN_BOLD(" --- trackNetintUsage(") "%s" CYAN_BOLD(") --- Thread: %d\n") \
			PR_VAR(PRIu32, interval)			\
			PR_VAR(PRIu16, type)				\
			PR_VAR("f", alert_usage)		\
			PR_VAR("f", percentage_usage)	\
			PR_VAR("zu", nii->down_cache)	\
			PR_VAR("zu", nii->up_cache)		\
			PR_VAR("s", down_up[0])			\
			PR_VAR("s", down_up[1])			\
			PR_VAR("zu", nii->down_bps)		\
			PR_VAR("zu", nii->up_bps)		\
			PR_VAR(PRId64, cache-time)		\
		CYAN_BOLD(" ---\n") 				\
		, nii->name, tc->id, tc->interval, nii->type, tc->alert_usage, percentage_usage, nii->down_cache, nii->up_cache, down_up[0], down_up[1], nii->down_bps, nii->up_bps, tc->cache_time
	);

#endif
			// tuptuO

			pthread_mutex_unlock(&Net_int_Mutex);

			sleep_ms(tc->interval); 

		}

		g_free(input_cmd);
		g_free(down_up);

	}  else {

		g_fprintf(STD, RED_BOLD("[ERROR]") " Network Interface values are not initialized.\n");
		return NULL;

	}

	return NULL;
	
}

void *trackFssUsage(void *thread_container){

	snapFss(thread_container); 

/*
*	Parameter Conversion
*/

	resource_thread_ty *tc = (resource_thread_ty *) thread_container;

	filesystems_ty *fss = (filesystems_ty *) tc->parameter;

	sleep_ms(tc->interval);

/*
*	Get RAM Usage Till The Program Stops
*/

	if( Init_State & is_Filesystems ){

		while( Program_State & ps_Running ){

			// Output
			if ( !(Program_Flag & pf_No_CLI_Output) ){

				pthread_mutex_lock(&File_sys_Mutex);

				CONSOLE_GOTO(0, Last_Thread_Id + 2);
				g_fprintf(STD, "Filesystems:");

				for(uint32_t i = 0; i < fss->count; i++){
					
					size_t total_size = ( (fss->info + i)->used + (fss->info + i)->available);
					
					gfloat percentage_usage = (fss->info + i)->used / (gfloat) total_size * 100.0;

					tc->output_line = Last_Thread_Id + 3 + i;
					CONSOLE_GOTO(0, tc->output_line);
					g_fprintf(STD, CONSOLE_ERASE_LINE " %s:\t%s / %s \t %2.2f%% \n", (fss->info + i)->partition, bytes_to_str((fss->info + i)->used), bytes_to_str(total_size), percentage_usage);

					if (percentage_usage >= tc->alert_usage){
						sendAlert(tc, percentage_usage);
					}

				}

				CONSOLE_GOTO(0, Last_Thread_Id + 1);
				g_fprintf(STD, CONSOLE_ERASE_LINE);

				fflush(STD);

				pthread_mutex_unlock(&File_sys_Mutex);

			}

#ifdef DEBUG_RUT
	pthread_mutex_lock(&File_sys_Mutex);
	
		for(uint32_t i = 0; i < fss->count; i++){
			
			size_t total_size = ( (fss->info + i)->used + (fss->info + i)->available);
			gfloat percentage_usage = (fss->info + i)->used / (gfloat) total_size * 100.0;

			if (percentage_usage >= tc->alert_usage){
				g_fprintf(STD, PINK_BOLD("[ALERT]") " Filesystem (%s) usage (%2.2f%%) is over %2.2f%% in last %" PRIu32 "ms!\n", (fss->info + i)->partition, percentage_usage, tc->alert_usage, tc->interval);
			}

			g_fprintf(STD,
				CYAN_BOLD(" --- trackFssUsage(%s) --- Thread: %d\n") \
					PR_VAR(PRIu32, FSS_Interval)	\
					PR_VAR("f", alert_usage)		\
					PR_VAR("f", percentage_usage)	\
					PR_VAR(PRId64, used)			\
					PR_VAR(PRId64, capacity)		\
					PR_VAR(PRId64, cache-time)		\
				CYAN_BOLD(" ---\n") 				\
				, (fss->info + i)->partition, tc->id, tc->interval, tc->alert_usage, percentage_usage, (fss->info + i)->used, total_size, tc->cache_time
			);
		}
	
	pthread_mutex_unlock(&File_sys_Mutex);
#endif

			// tuptuO


			sleep_ms(tc->interval);


			pthread_mutex_lock(&File_sys_Mutex);

			tc->cache_time = getPhysicalFilesystems(fss);

			pthread_mutex_unlock(&File_sys_Mutex);

		}

	} else {

		g_fprintf(STD, RED_BOLD("[ERROR]") " Filesystems are not initialized.\n");
		return NULL;

	}

	return NULL;
}

/*
*
*/

/*
* Get CPU's snapshot
* grep cpu /proc/stat
*/
gint64 getCpuTimings(uint32_t *cpu_total, uint32_t *cpu_idle, REQUIRE_WITH_SIZE(gchar *, cpu_identifier)){

	*cpu_total = 0;
	*cpu_idle = 0;

	const gchar delim[2] = " ";
	uint8_t i = 0;
	gchar 	*token 	= NULL,
			*temp 	= NULL;


	readSearchGetFirstLine(&temp, PATH_CPU_STATS, PASS_WITH_SIZE_VAR(cpu_identifier), 1, PASS_WITH_SIZEOF(" "));

	gint64 read_time = g_get_monotonic_time();

	strtok(temp, delim);
	token = strtok(NULL, delim); // skip cpu_id
	
	while ( token != NULL ){

		*cpu_total += atol(token);
		token = strtok(NULL, delim);

		if (++i == 3){

			if (token) {

				*cpu_idle = atol(token);

			} else {

				g_free(temp);
				*cpu_total = 0;
				*cpu_idle = 0;
				
				g_fprintf(STD, RED_BOLD("[ERROR]") " Could NOT parse: getCpuTimings\n");
				return;
			}

		}

	}// while

	g_free(temp);
	return read_time;
}

//Alternative: tail -n +3 /proc/net/dev | awk '{print $1}' | sed 's/.$//'
gint64 getNetworkInterfaces(resources_ty *netints){

	// Get Interface List
	gchar **netints_temp = NULL;

	gchar *command_output = run_command("ls /sys/class/net/");  //hold output for percise timing

	gint64 read_time = g_get_monotonic_time();

	str_split(&netints_temp, command_output, '\n', (size_t *) &netints->count);

	command_output = NULL;


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


	netints->threads = (resource_thread_ty *)g_malloc(netints->count * sizeof(resource_thread_ty));

	struct net_int_info *nii = (struct net_int_info *)g_malloc(netints->count * sizeof(struct net_int_info));

	for (uint16_t i = 0; i < netints->count; i++){
		
		(nii + i)->name = *(netints_temp + i);

	}

	g_free(netints_temp);


	// Get Specs
	gchar *path = NULL;
	FILE *net_file = NULL;

	uint16_t 	filtered_index = 0,
				arphdr_no = UINT16_MAX;

	for (uint16_t i = 0; i < netints->count; i++){

		// Get Type
		path = realloc(path, sizeof("/sys/class/net//type") + sizeof((nii + i)->name));
		
		g_stpcpy(path, "/sys/class/net/");
		strcat(path, (nii + i)->name);
		strcat(path, "/type");

		net_file = fopen(path, "r");

		fscanf(net_file, "%" PRIu16, &arphdr_no);


		// Discard if not an ARP Hardware interface
		if ( arphdr_no <= ARPHRD_INFINIBAND){

			// Assign to the list
			(nii + filtered_index)-> name = (nii + i)->name;

			(nii + filtered_index)-> type = arphdr_no;


			// Get Bandwith
			path = realloc(path, sizeof("/sys/class/net//speed") + sizeof((nii + filtered_index)->name));

			g_stpcpy(path, "/sys/class/net/");
			strcat(path, (nii + filtered_index)->name);
			strcat(path, "/speed");

			net_file = fopen(path, "r");


			fscanf(net_file, "%zu", &(nii + filtered_index)->bandwith_mbps);

			(netints->threads + filtered_index)->parameter = (void *) (nii + filtered_index);

			filtered_index++;


		} else {
			(netints->count)--; //todo dealloc
		}

	}

	g_free(path);
	fclose(net_file);

#ifdef DEBUG_RUT

	g_fprintf(STD, CYAN_BOLD(" --- getNetworkInterfaces() ---\n"));

	SOUT("d", netints->count);

	g_fprintf(STD, CYAN_BOLD("Network Interfaces:\n"));

	for(uint16_t i = 0 ; i < netints->count; i++){
		g_fprintf(STD, CYAN_BOLD("- Name:") " %s\n\t" CYAN_BOLD("Type:") " %d\n\t" CYAN_BOLD("Bandwith:") " %lu Mbps\n", (nii + i)->name, (nii + i)->type, (nii + i)->bandwith_mbps);
	}

	g_fprintf(STD, CYAN_BOLD(" ---\n"));

#endif

	return read_time;

}

/* 
*  Get all disks from PATH_DISK_STATS
*  Get disks names and maj if minor no is == 0
*  $ cat "PATH_DISK_STATS" | awk '$2 == 0 {print $3}'
*/
gint64 getAllDisks(resources_ty *disks){

	gchar **temp = NULL;

	gchar *command_output = run_command("awk '$2 == 0 {print $3}' "PATH_DISK_STATS);  //hold output for percise timing

	gint64 read_time = g_get_monotonic_time();

	str_split(&temp , command_output, '\n', (size_t *) &disks->count);

	command_output = NULL;


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


	disks->threads = (resource_thread_ty *)g_malloc(disks->count * sizeof(resource_thread_ty));

	struct disk_info *di = (struct disk_info *)g_malloc(disks->count * sizeof(struct disk_info));

	for (i = 0 ; i < disks->count; i++){
		
		(di+i)->name = *(temp + i);

		(disks->threads + i)->parameter = (void *) (di + i);
	}

	g_free(temp);


#ifdef DEBUG_RUT
	g_fprintf(STD, CYAN_BOLD(" --- getAllDisks() ---\n"));

	SOUT("d", disks->count);

	g_fprintf(STD, CYAN_BOLD("Disk Names:\n"));

	for(uint16_t i = 0 ; i < disks->count; i++){
		g_fprintf(STD, CYAN_BOLD("-")"%s\n", (di+i)->name);
	}

	g_fprintf(STD, CYAN_BOLD(" ---\n"));
#endif

	return read_time;

}

// df --type btrfs --type ext4 --type ext3 --type ext2 --type vfat --type iso9660 --block-size=1 | tail -n +2 | awk {'print $1" "$2" "$3" "$4'}
gint64 getPhysicalFilesystems(filesystems_ty *filesystems){

	gchar **filesystems_temp = NULL;
	
	gchar *command_output = run_command("df --type btrfs --type ext4 --type ext3 --type ext2 --type vfat --type iso9660 --block-size=1 | tail -n +2 | awk {'print $1\" \"$2\" \"$3\" \"$4\" X\"'}");// hold output for percise timing

	gint64 read_time = g_get_monotonic_time();

	str_split(&filesystems_temp, command_output, '\n', (size_t *) &filesystems->count);

	command_output = NULL;


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
	gchar **fsi_temp = NULL;
	for (i = 0; i < filesystems->count; i++){

		(*(filesystems + i)).info = (struct filesystem_info *)g_malloc(sizeof(struct filesystem_info));

		str_split(&fsi_temp, *(filesystems_temp + i), ' ', &info_field_count );

		(filesystems->info + i)->partition 	= *(fsi_temp + 0);

		str_to_uint64(*(fsi_temp + 1), &(filesystems->info + i)->block_size ); // as 1-byte sizes
		str_to_uint64(*(fsi_temp + 2), &(filesystems->info + i)->used );
		str_to_uint64(*(fsi_temp + 3), &(filesystems->info + i)->available );

	}


	g_free(fsi_temp);
	g_free(filesystems_temp);


#ifdef DEBUG_RUT

		g_fprintf(STD, CYAN_BOLD(" --- getPhysicalFilesystems() ---\n"));

		SOUT("d", filesystems->count);
		g_fprintf(STD, CYAN_BOLD("Filesystems:\n"));

		for(uint16_t i = 0 ; i < filesystems->count; i++){

			g_fprintf(STD, CYAN_BOLD("- Partition: ")"\t%s\n", (*((*filesystems).info + i)).partition);
			g_fprintf(STD, CYAN_BOLD("- Byte blocks: ")"\t%zu\n", (*((*filesystems).info + i)).block_size);
			g_fprintf(STD, CYAN_BOLD("- Used: ")"\t%zu\n", (*((*filesystems).info + i)).used);
			g_fprintf(STD, CYAN_BOLD("- Available: ")"\t%zu\n", (*((*filesystems).info + i)).available);

		}

		g_fprintf(STD, CYAN_BOLD(" ---\n"));
#endif


	return read_time;

}

// Find the first VARIABLE in PATH and return its numeric value
int64_t getFirstVarNumValue( const gchar* path, REQUIRE_WITH_SIZE(const gchar*, variable), const uint16_t variable_column_no ){
	uint64_t output = 0;
	const gchar delim[2] = " ";
	gchar 	*token 	= NULL,
			*temp 	= NULL;

	readSearchGetFirstLine(&temp, path, PASS_WITH_SIZE_VAR(variable), variable_column_no, PASS_WITH_SIZEOF(delim));

	strtok(temp, delim);
	token = strtok(NULL, delim); // skip VARIABLE
	

	if (token) {
		output = atoll(token) * KILOBYTE; // Convert to BYTE
	} else {
		g_fprintf(STD, RED_BOLD("[ERROR]") " Could NOT parse the value of: "RED_BOLD("%s")"\n", variable);
		return 0;
	}
#ifdef DEBUG_RUT
	g_fprintf(STD, CYAN_BOLD("getFirstVarNumValue()")" | %s = %" PRId64 "\n", variable, output);
#endif
	g_free(temp);
	return output;
}

/*
*
*/

// Get the current OS disk
gchar* getSystemDisk(gchar* os_partition_name, gchar* maj_no){

	os_partition_name = NULL;
	maj_no = NULL;
	
	gchar 	*output = NULL,
			*temp 	= NULL;

	readSearchGetFirstLine(&temp, PATH_MOUNT_STATS, PASS_WITH_SIZEOF("/"), 5, PASS_WITH_SIZEOF(" "));

	getColumn(
		&os_partition_name,
		temp,
		2,
		PASS_WITH_SIZEOF(" ")
	);

	g_free(temp);
	temp = (gchar *) g_malloc (sizeof(gchar));

	const uint16_t os_partition_name_size = leftTrimTill(os_partition_name, '/'); // Trim path
	readSearchGetFirstLine(&temp, PATH_DISK_STATS, PASS_WITH_SIZE_VAR(os_partition_name), 3, PASS_WITH_SIZEOF(" "));

	getColumn(
		&maj_no,
		temp,
		1,
		PASS_WITH_SIZEOF(" ")
	);

	g_free(temp);
	temp = (gchar *) g_malloc (sizeof(gchar));

	readSearchGetFirstLine(&temp, PATH_DISK_STATS, PASS_WITH_SIZEOF(maj_no), 1, PASS_WITH_SIZEOF(" "));

	getColumn(
		&output,
		temp,
		3,
		PASS_WITH_SIZEOF(" ")
	);

	g_free(temp);

#ifdef DEBUG_RUT
	g_fprintf(STD, CYAN_BOLD("getSystemDisk()")" | output = %s | os_partition_name = %s | maj_no = %s\n", output, os_partition_name, maj_no);
#endif

	return output;
}


// End