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
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

// Compare functions for our scheduler

int compare_L1 (Thread* x, Thread* y) {
    double _x = x->getRemainingBurstTime(), _y = y->getRemainingBurstTime();
    if (_x > _y) return 1; // Remaining time as less as possible
    else if (_x == _y) return 0;
    else return -1;
}

int compare_L2 (Thread* x, Thread* y) {
     int _x = x->getPriority(), _y = y->getPriority();
    if (_x < _y) return 1; // Priority as much as possible
    else if (_x == _y) return 0;
    else return -1;
}

// Supporting functions to calculate burst time and current thread run time

int Scheduler::RunTime() {
    return kernel->stats->totalTicks - threadStartTick;
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    // Default layer set to 3
    currentLayer = 3;
    readyList_L1 = new SortedList<Thread *>(compare_L1);
    readyList_L2 = new SortedList<Thread *>(compare_L2);
    readyList_L3 = new List<Thread *>;
    threadStartTick = kernel->stats->totalTicks;
    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList_L1; 
    delete readyList_L2;
    delete readyList_L3;  
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun (Thread *thread) {
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    // Start to waiting
    thread->setStatus(READY);
    thread->setWaiting(kernel->stats->totalTicks);

    // Inserting into different layers
    int priority = thread->getPriority(), level;
    if (priority >= 0 && priority < 50) {
        readyList_L3->Append(thread);
        level = 3;
    } else if (priority >= 50 && priority < 100) {
        readyList_L2->Insert(thread);
        level = 2;
    } else if (priority >= 100 && priority < 150) {
        readyList_L1->Insert(thread);
        level = 1;
    } else {
        cout << "Invalid priority\n";
        Abort();
    }
    // Debug message
    DEBUG(dbgMP3, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << /* ", "  << thread->getName() << */ "] is inserted into queue L[" << level << "]");
}

//----------------------------------------------------------------------
// Scheduler::UpdateQueues
//  The part that updates each queues.
//  Including try to update priority, and aging mechanism.
//----------------------------------------------------------------------

void Scheduler::UpdateQueues() {
    // L1: Update Priority if necessary
    int time = kernel->stats->totalTicks;
    ListIterator<Thread *> iter1(readyList_L1); // It's also ok to not update in L1
    for (; !iter1.IsDone(); iter1.Next()) 
        iter1.Item()->updatePriority(time);

    // L2: We need to record those threads that update their priority
    ListIterator<Thread *> iter2(readyList_L2); 
    List<Thread *> uplevel2; // Threads that goes to L1
    List<Thread *> upgrade; // Threads that has updated their priority
    // First update the priorities
    for (; !iter2.IsDone(); iter2.Next()) {
        int old = iter2.Item()->getPriority();
        int priority = iter2.Item()->updatePriority(time);
        if (old != priority) {
            if (priority >= 100) // update of level
                uplevel2.Append(iter2.Item());
            else 
                upgrade.Append(iter2.Item());
        }
    }
    // Then insert to corresponding queues
    ListIterator<Thread *> uplevel2Iter(&uplevel2);
    for (; !uplevel2Iter.IsDone(); uplevel2Iter.Next()) {
        readyList_L2->Remove(uplevel2Iter.Item());
        DEBUG(dbgMP3, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << uplevel2Iter.Item()->getID() << /* ", " << uplevel2Iter.Item()->getName() << */ "] is removed from queue L[2]");

        readyList_L1->Insert(uplevel2Iter.Item());
        DEBUG(dbgMP3, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << uplevel2Iter.Item()->getID() << /* ", " << uplevel2Iter.Item()->getName() << */ "] is inserted into queue L[1]");
    }
    ListIterator<Thread *> upgradeIter(&upgrade);
    for (; !upgradeIter.IsDone(); upgradeIter.Next()) {
        readyList_L2->Remove(upgradeIter.Item());
        DEBUG(dbgMP3, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << upgradeIter.Item()->getID() << /* ", " << upgradeIter.Item()->getName() << */ "] is removed from queue L[2]");
        
        readyList_L2->Insert(upgradeIter.Item());
        DEBUG(dbgMP3, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << upgradeIter.Item()->getID() << /* ", " << upgradeIter.Item()->getName() << */ "] is inserted into queue L[2]");
    }

    // L3: Similarly use a list to store who will go uplevel
    ListIterator<Thread *> iter3(readyList_L3); 
    List<Thread *> uplevel3;
    for (; !iter3.IsDone(); iter3.Next()) {
        int priority = iter3.Item()->updatePriority(time);
        if (priority >= 50)  // update of level 
            uplevel3.Append(iter3.Item());
    }
    ListIterator<Thread *> uplevel3Iter(&uplevel3);
    for (; !uplevel3Iter.IsDone(); uplevel3Iter.Next()) {
        readyList_L3->Remove(uplevel3Iter.Item());
        DEBUG(dbgMP3, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << uplevel3Iter.Item()->getID() << /* ", " << uplevel3Iter.Item()->getName() << */ "] is removed from queue L[3]");

        readyList_L2->Insert(uplevel3Iter.Item());
        DEBUG(dbgMP3, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << uplevel3Iter.Item()->getID() << /* ", " << uplevel3Iter.Item()->getName() << */ "] is inserted into queue L[2]");
    }
}

//----------------------------------------------------------------------
// Scheduler::ToYield
// 	Afther updating, return whether to yield to other thread at current tick.
//  Call this function at Alarm::CallBack base on the following reason:
//
//      (f) The operations of preemption and priority updating MUST be 
//          delayed until the next timer alarm interval in alarm.cc Alarm::Callback.
//
//----------------------------------------------------------------------

bool Scheduler::ToYield () {
    bool yield;
    // Since we don't update current thread T, so we need deduct run time here
    int currentThreadRemainTime = kernel->currentThread->getRemainingBurstTime() - RunTime();
    // If L1 is empty, it is ok to be any number
    int firstL1ThreadRemainTime = readyList_L1->IsEmpty() ? 0 : readyList_L1->Front()->getRemainingBurstTime();

    // Time quantum is done
    if (currentLayer == 3 && RunTime() >= 100)
        yield = TRUE;
    // Preemption
    else if (currentLayer == 3 && !(readyList_L1->IsEmpty() && readyList_L2->IsEmpty()))
        yield = TRUE;
    else if (currentLayer == 2 && !readyList_L1->IsEmpty())
        yield = TRUE;
    else if (currentLayer == 1 && !readyList_L1->IsEmpty() && firstL1ThreadRemainTime < currentThreadRemainTime)
        yield = TRUE;
    // No need to Yield
    else
        yield = FALSE;

    return yield;
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread* Scheduler::FindNextToRun () {
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    Thread* thread;
    if (readyList_L1->IsEmpty()) {
        if (readyList_L2->IsEmpty()) {
            if (readyList_L3->IsEmpty())
                // All lists are empty
                thread = NULL;
            else {
                // Next thread ls from layer 3
                currentLayer = 3;
                thread = readyList_L3->RemoveFront();
            }
        } else {
            // Next thread is from layer 2
            currentLayer = 2;
            thread = readyList_L2->RemoveFront();

        }
    } else {
        // Next thread is from layer 1
        currentLayer = 1;
        thread = readyList_L1->RemoveFront();
    }

    // Debug message 
    if (thread != NULL)
        DEBUG(dbgMP3, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << /* ", "  << thread->getName() << */ "] is removed from queue L[" << currentLayer << "]");
    
    return thread;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    DEBUG(dbgMP3, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << /* ", "  << nextThread->getName() << */ "] is now selected for execution, thread [" << oldThread->getID() << /* ", "  << oldThread->getName() << */ "] is replaced, and it has executed [" << RunTime() << "] ticks");
    threadStartTick = kernel->stats->totalTicks;

    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "ReadyList_L1 contents: ";
    readyList_L1->Apply(ThreadPrintL1);
    cout << "\n";
    cout << "ReadyList_L2 contents: ";
    readyList_L2->Apply(ThreadPrint);
    cout << "\n";
    cout << "ReadyList_L3 contents: ";
    readyList_L3->Apply(ThreadPrint);
    cout << "\n";
}
