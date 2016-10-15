// progtest.cc 
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.  
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"
#include "syscall.h"
#include "scheduler.h"

//----------------------------------------------------------------------
// StartUserProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void
StartUserProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename);
    ProcessAddrSpace *space;

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    space = new ProcessAddrSpace(executable);    
    currentThread->space = space;

    delete executable;			// close file

    space->InitUserCPURegisters();		// set the initial register values
    space->RestoreStateOnSwitch();		// load page table register

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void 
ConsoleTest (char *in, char *out)
{
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    
    for (;;) {
	readAvail->P();		// wait for character to arrive
	ch = console->GetChar();
	console->PutChar(ch);	// echo it!
	writeDone->P() ;        // wait for write to finish
	if (ch == 'q') return;  // if q, quit
    }
}


void
ForkStartFunctionBatch (int dummy)
{
   currentThread->Startup();
   machine->Run();
}


void
ExecIndCommands(char *filename, int priority) {
    OpenFile *executable = fileSystem->Open(filename);
    if (executable == NULL) {
        printf("Unable to open file %s\n", filename);
        return;
    }

    NachOSThread *currThread = new NachOSThread(filename, priority);
    ProcessAddrSpace *space = new ProcessAddrSpace(executable);
    currThread->space = space;

    delete executable;          // close file

    currThread->space->InitUserCPURegisters();      // set the initial register values
    currThread->space->RestoreStateOnSwitch();      // load page table register
    currThread->SaveUserState();
    currThread->AllocateThreadStack(ForkStartFunctionBatch,0);
    currThread->Schedule();
}

void
ExecFileCommands (char *filename)
{
    OpenFile *dataFile = fileSystem->Open(filename);

    if (dataFile == NULL) {
        printf("Unable to open file %s\n", filename);
        return;
    }

    int lengthOfFile = dataFile->Length();

    char data[lengthOfFile];
    int outLength = dataFile->Read(data, lengthOfFile);
    ASSERT(outLength == lengthOfFile);
    
    int i = 0, j = 0;
    char execFile[50];
    char priority[5];
    char schedCode[5];

    // first line is for schedulerCode
    while (i < lengthOfFile && data[i] != '\n') {
        schedCode[j] = data[i];
        i++;
        j++;
    }
    schedCode[j] = '\0';
    scheduler->schedulerCode = atoi(schedCode);

    i++;
    while (i < lengthOfFile) {
        j = 0;
        while (i < lengthOfFile && data[i] != ' ' && data[i] != '\n') {
            execFile[j] = data[i];
            //printf("%c", execFileTemp[j]);
            j++;
            i++;
        }
        execFile[j] = '\0';

        if (data[i] == '\n')
        {
            i++;
            priority[0] = '1';   // if priority is not mentioned default = 100
            priority[1] = '0';
            priority[2] = '0';
            priority[3]='\0';
            //j=0;
        }
        else {
            i++;
            j = 0;
            
            while (i < lengthOfFile && data[i] != '\n') {
                priority[j] = data[i];
                j++;
                i++;
            }
            // priority[j] = '\0';
        }
        i++;    // next line
        int priority_val = atoi(priority);
        ExecIndCommands(execFile, priority_val);
    }
    delete dataFile;

    //code from system call Exit
    
    exitThreadArray[currentThread->GetPID()] = true;

    // Find out if all threads have called exit
    for (i=0; i<thread_index; i++) {
        if (!exitThreadArray[i]) break;
    }
    currentThread->Exit(i==thread_index, 0);
}