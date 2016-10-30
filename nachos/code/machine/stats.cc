// stats.h 
//	Routines for managing statistics about Nachos performance.
//
// DO NOT CHANGE -- these stats are maintained by the machine emulation.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "stats.h"

//----------------------------------------------------------------------
// Statistics::Statistics
// 	Initialize performance metrics to zero, at system startup.
//----------------------------------------------------------------------

Statistics::Statistics()
{
    totalTicks = idleTicks = systemTicks = userTicks = 0;
    numDiskReads = numDiskWrites = 0;
    numConsoleCharsRead = numConsoleCharsWritten = 0;
    numPageFaults = numPacketsSent = numPacketsRecvd = 0;
}

//----------------------------------------------------------------------
// Statistics::Print
// 	Print performance metrics, when we've finished everything
//	at system shutdown.
//----------------------------------------------------------------------

void
Statistics::Print()
{

    double util = (systemTicks + userTicks)/(double)totalTicks;


    avg_wait=wait_time_total/double(thread_count);
    printf("\nThread Wait Statistics\n");
    printf("Total wait time=%d\n",wait_time_total);
    printf("Thread count =%d\n",thread_count);
    printf("Average wait time %f\n", avg_wait);



    printf("\nCPU Usage Statistics\n");
    printf("Maximum CPU Burst %d\n", cpu_burst_max);
    printf("Minimum CPU Burst %d\n", cpu_burst_min);
    printf("CPU Burst count %d\n", cpu_burst_count);
    printf("Total CPU Burst time %d\n", cpu_burst_total);
    printf("CPU Utilization: %f\n", util);



    printf("Ticks: total %d, idle %d, system %d, user %d\n", totalTicks, 
	idleTicks, systemTicks, userTicks);
    printf("Disk I/O: reads %d, writes %d\n", numDiskReads, numDiskWrites);
    printf("Console I/O: reads %d, writes %d\n", numConsoleCharsRead, 
	numConsoleCharsWritten);
    printf("Paging: faults %d\n", numPageFaults);
    printf("Network I/O: packets received %d, sent %d\n", numPacketsRecvd, 
	numPacketsSent);
}
