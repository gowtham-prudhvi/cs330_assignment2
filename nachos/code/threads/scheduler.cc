// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextThreadToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

//----------------------------------------------------------------------
// NachOSscheduler::NachOSscheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

NachOSscheduler::NachOSscheduler()
{ 
    readyThreadList = new List;

    //SJF
    alpha = 0.5;

    quantum = 0;

    int i;
    for (i=0; i < MAX_THREAD_COUNT; i++) {
        cpuCount[i] = 0;
    } 
} 

//----------------------------------------------------------------------
// NachOSscheduler::~NachOSscheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

NachOSscheduler::~NachOSscheduler()
{ 
    delete readyThreadList; 
} 

//----------------------------------------------------------------------
// NachOSscheduler::ThreadIsReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
NachOSscheduler::ThreadIsReadyToRun (NachOSThread *thread)
{
    DEBUG('t', "Putting thread %s with PID %d on ready list.\n", thread->getName(), thread->GetPID());

    thread->setStatus(READY);
    thread->curr_wait_start=stats->totalTicks;
    
    // SJF and non zero cpu burst
    if (schedulerCode == 2 && thread->prev_cpu_burst > 0) {
        double expected_cpu_burst = alpha * thread->prev_cpu_burst + (1 - alpha) * thread->prev_expected_cpu_burst;

        int error = thread->prev_cpu_burst - expected_cpu_burst;

        if (error < 0)
            error = -1 * error;

        // TODO: report stats-error

        //update prev_expected_cpu_burst
        thread->prev_expected_cpu_burst = expected_cpu_burst;

        // Insert the thread into the List using expected_cpu_burst
        readyThreadList->SortedInsert((void*)thread, expected_cpu_burst);
    }
    else {
        //printf("pid of thread = %d\n", thread->GetPID());
        readyThreadList->Append((void *)thread);
    }
}

//----------------------------------------------------------------------
// NachOSscheduler::FindNextThreadToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

NachOSThread *
NachOSscheduler::FindNextThreadToRun ()
{
    // default except for UNIX scheduling
    if (schedulerCode <= 6)
        return (NachOSThread *)readyThreadList->Remove();
    else {
        // if readyThreadList is empty return NULL
        if (readyThreadList->first == NULL)
            return NULL;

        // get the thread with minimum priority value
        // scan the entire readyThreadList and update minElementPtr accordingly
        ListElement *minElementPtr = readyThreadList->first;

        // temp variables are used to iterate through the readyThreadList
        ListElement *tempElementPtr = minElementPtr;
        NachOSThread *tempThreadPtr = (NachOSThread*)minElementPtr->item;
        int tempPriorityValue = tempThreadPtr->priority;

        NachOSThread *minThreadPtr = tempThreadPtr;
        int minPriorityValue =  minThreadPtr->priority;

        // iterate through readyThreadList
        while (tempElementPtr != NULL) {
            tempThreadPtr = (NachOSThread*)tempElementPtr->item;
            tempPriorityValue = tempThreadPtr->priority;

            if (tempPriorityValue < minPriorityValue) {
                minElementPtr = tempElementPtr;
                minThreadPtr = tempThreadPtr;
                minPriorityValue = tempPriorityValue;
            }
            tempElementPtr = tempElementPtr->next;
        }

        // remove the min element from readyThreadList
        tempElementPtr = readyThreadList->first;
        
        // if the minThread is the first then directly use default Remove()
        if (tempElementPtr == minElementPtr) {
            return (NachOSThread *)readyThreadList->Remove();
        }

        ListElement *prevElementPtr = tempElementPtr;
        tempElementPtr = tempElementPtr->next;
        while(tempElementPtr != NULL) {
            if(tempElementPtr == minElementPtr) {
                prevElementPtr->next = tempElementPtr->next;

                // if the min is last then update readyThreadList last as prevElementPtr
                if (tempElementPtr == readyThreadList->last) {
                    readyThreadList->last = prevElementPtr;
                }
            }
            prevElementPtr = tempElementPtr;
            tempElementPtr = tempElementPtr->next;
        }

        delete minElementPtr;
        return minThreadPtr;
    }
}

//----------------------------------------------------------------------
// NachOSscheduler::Schedule
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
NachOSscheduler::Schedule (NachOSThread *nextThread)
{
    NachOSThread *oldThread = currentThread;
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveStateOnSwitch();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING); 
    //stats->cpu_burst_count++;     // nextThread is now running
    nextThread->wait_time_sum += stats->totalTicks-(nextThread->curr_wait_start);
//    printf("wait_time_sum=%d\n",nextThread->wait_time_sum );

    nextThread->curr_cpu_burst_start=stats->totalTicks;
    currentThread->cpu_burst_count++;
    //printf("cpu_start=%d count=%d\t", currentThread->curr_cpu_burst_start,currentThread->cpu_burst_count);
    
    DEBUG('t', "Switching from thread \"%s\" with pid %d to thread \"%s\" with pid %d\n",
	  oldThread->getName(), oldThread->GetPID(), nextThread->getName(), nextThread->GetPID());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    _SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\" with pid %d\n", currentThread->getName(), currentThread->GetPID());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreStateOnSwitch();
    }
#endif
}

//----------------------------------------------------------------------
// NachOSscheduler::Tail
//      This is the portion of NachOSscheduler::Schedule after _SWITCH(). This needs
//      to be executed in the startup function used in fork().
//----------------------------------------------------------------------

void
NachOSscheduler::Tail ()
{
    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in NachOSThread::FinishThread()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
        threadToBeDestroyed = NULL;
    }

#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {         // if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
        currentThread->space->RestoreStateOnSwitch();
    }
#endif
}

//----------------------------------------------------------------------
// NachOSscheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
NachOSscheduler::Print()
{
    printf("Ready list contents:\n");
    readyThreadList->Mapcar((VoidFunctionPtr) ThreadPrint);
}
