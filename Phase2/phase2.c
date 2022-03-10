/* ------------------------------------------------------------------------
    phase2.c
    Applied Technology
    College of Applied Science and Technology
    The University of Arizona
    CSCV 452

    Kiera Conway
    Katelyn Griffith

    NOTES:
        UPDATE clearMboxSlot NAME ***************
            clearMboxSlot - clears ALL slots
            RemoveSlotLL -  removes single Slot
                rename one or both to prevent confusion*******
        Going to need to work on phase2.c and handler.c
        term files work like message queues for phase 2
        need symbollic link for phase 2?
        need libmjdphase1 file for phase 2
        update makefile for phase1lib to mjdphase2

        check_kernel_mode
        enableInterrupts
        disableInterupts
   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
int check_io();

/* Helper Function for Send and Receive */
void HelperSend(int mbox_index, void *msg, int msg_size, int block_flag);
void HelperReceive(int mbox_index, void *msg, int msg_size, int block_flag);

/* Functions Brought in From Phase 1 */
static void enableInterrupts();
static void disableInterrupts();
static void check_kernel_mode(const char *functionName);
void DebugConsole2(char *format, ...);
void Time_Slice(void);

/* Handler Initializers */
void ClockIntHandler2(int dev, void *arg);
void DiskIntHandler(int dev, void *arg);
void TermIntHandler(int dev, void *arg);
void SyscallIntHandler(int dev, void *arg);

/*Mailbox Table*/
int GetNextMailID();
void InitializeMailbox(int newStatus, int mbox_index, int mbox_id, int maxSlots, int slotSize);
int GetMboxIndex(int mbox_id);

/* Linked List Functions */
//General, Multi-use
void InitializeList(slotQueue *pSlot, procQueue *pProc);            //Initialize Linked List
bool ListIsFull(const slotQueue *pSlot, const procQueue *pProc);    //Check if Slot List is Full
bool ListIsEmpty(const slotQueue *pSlot, const procQueue *pProc);   //Check if Slot List is Empty

//Slot Lists
int AddSlotLL(void *msg_ptr, int msg_size, mailbox * mbox); //Add Slot to Specific Mailbox
int RemoveSlotLL(slotQueue * pSlotQ);                       //Remove Slot from Specific Mailbox

//Process Lists
void AddProcessLL(int pid, procQueue * pq);         //Add pid to Process Queue
int RemoveProcessLL(int pid, procQueue * pq);      //Remove pid from Process Queue

/* Mailbox Slots Release Functions */
void HelperRelease(mailbox *mbox);          //Clears all slots in mailbox, calls InitializeMailbox
void UnblockProcs(ProcList *pSaveProc);     //Iterates through process list, unblocks

/* -------------------------- Globals ------------------------------------- */

int debugFlag2 = 0;                 //used for debugging
mailbox MailboxTable[MAXMBOX];      //mailbox table
int totalMailbox;                   //total mailboxes
int totalSlots = 0;                 //total slot count
int totalActiveSlots = 0;           //total active slot count
unsigned int next_mid = 0;          //next mailbox id [mbox_id]
int clock_mbox;                     //clock interrupt handler
int disk_mbox[2];                   //disk interrupt handler
int term_mbox[4];                   //terminal interrupt handler

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
  Name -           start1
  Purpose -        Initializes mailboxes and interrupt vector,
                   Starts the phase2 test process.
  Parameters -     *arg:   default arg passed by fork1, not used here.
  Returns -        0:      indicates normal quit
  Side Effects -   lots since it initializes the phase2 data structures.
  ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kid_pid, status;

    DebugConsole2("start1(): at beginning\n");

    /* Check for kernel mode */
    check_kernel_mode("start1");

    /* Disable interrupts */
    disableInterrupts();

    /*Initialize the mailbox table*/
    memset(MailboxTable, 0, sizeof(MailboxTable)); // added in kg

    /*Initialize individual interrupts in the interrupt vector*/
    //values from usloss.h, handler.c
    int_vec[CLOCK_INT] = &clock_handler;      // 0 - clock
    int_vec[DISK_INT] = &disk_handler;        // 2 - disk
    int_vec[TERM_INT] = &term_handler;        // 3 - terminal
    int_vec[SYSCALL_INT] = &syscall_handler;  // 5 - syscall

    /*Initialize the system call vector*/
//    for(int i = 0; i < MAXSYSCALLS; i++){
//        sys_vec[i] = nullsys;
//    }

    /*allocate mailboxes for interrupt handlers */
    clock_mbox = MboxCreate(0, sizeof(int));    //Clock
    disk_mbox[0] = MboxCreate(0, sizeof(int));  //Disk
    disk_mbox[1] = MboxCreate(0, sizeof(int));
    term_mbox[0] = MboxCreate(0, sizeof(int));  //Terminal
    term_mbox[1] = MboxCreate(0, sizeof(int));
    term_mbox[2] = MboxCreate(0, sizeof(int));
    term_mbox[3] = MboxCreate(0, sizeof(int));

    /* Enable interrupts */
    enableInterrupts();

    DebugConsole2("start1(): fork'ing start2 process\n");

    /*Create Process for start2*/
    kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);

    /*Join until Start2 quits*/
    if ( join(&status) != kid_pid ) {
        console("start2(): join returned something other than start2's pid\n");
        halt(1);
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name -           MboxCreate
   Purpose -        Creates a mailbox
   Parameters -     slots:      maximum number of mailbox slots
                    slot_size:   max size of a msg sent to the mailbox
   Returns -        -1: no mailbox available, slot size incorrect
                  >= 0: Unique mailbox ID
   Side Effects -   initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    int newMid;     //new mailbox id
    int mbox_index;  //new mailbox index in Mailbox Table

    // Check for kernel mode
    check_kernel_mode(__func__);

    // Disable interrupts
    disableInterrupts();

    /*Error Check: No Mailboxes Available*/
    if(totalMailbox >= MAXMBOX){
        DebugConsole2("%s : No Mailboxes Available.\n", __func__);
        return -1;
    }

    /*Error Check: Incorrect Slot Size*/
    if (slot_size > MAXMESSAGE)
    {
        DebugConsole2("%s : The message size[%d] exceeds limit [%d].\n",
                      __func__, slots, MAXMESSAGE);
        return -1;
    }

    /*Error Check: Check for other invalid parameter values*/
    if (slots < 0 || slot_size < 0) //todo: fix error message
    {
        DebugConsole2("%s : An invalid value of slot size [%d] or message size [%d] was used.\n",
                      __func__, slots, slot_size);
        return -1;
    }

//    // Check if zero-slot mailbox
//    if(slots == 0){
//        /*TODO
//         * Zero-slot mailboxes are a special type of mailbox and are intended for
//         * synchronization between a sender and a receiver. These mailboxes have
//         * no slots and are used to pass messages directly between processes;
//         * they act much like a signal.
//         */
//    }

    /*Get next empty mailbox from mailboxTable*/
    if((newMid = GetNextMailID()) == -1){           //get next mbox ID
        //Error Check: Exceeding Max Processes
        DebugConsole2("Error: Attempting to exceed MAXMBOX limit [%d].\n", MAXMBOX);
        return -1;
    }
    mbox_index = newMid % MAXMBOX;                   //assign mbox_index

    /*Initialize Mailbox*/
    InitializeMailbox(MBOX_USED, mbox_index, newMid, slots, slot_size);

    /*Increment totalSlots*/
    totalSlots += slots;    //note: totalSlots is different than activeSlots

    /*Increment Mailbox*/
    totalMailbox += 1;

    // Enable interrupts
    enableInterrupts();

    /*Return Unique mailbox ID*/
    return MailboxTable[mbox_index].mbox_id;

} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name -           MboxSend
   Purpose -        Send a message to a mailbox
   Parameters -     mbox_id:    Unique mailbox ID
                    msg_ptr:    pointer to the message
                    msg_size:   size of message in bytes
   Returns -        -3: process is zapped,
                        mailbox was released while the process
                        was blocked on the mailbox.
                    -1: Invalid Parameters Given
                     0: Message Sent Successfully
   Side Effects -   none
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    //////////////////////////
    //TODO
    //
    // NOTES: add to list function
    //messages are delivered in the order sent
    //////////////////////////

    /*** Function Initialization ***/
    check_kernel_mode(__func__);    // Check for kernel mode
    disableInterrupts();                        // Disable interrupts
    int mboxIndex = GetMboxIndex(mbox_id);  //Get Mailbox index in MailboxTable
    mailbox *pMbox = &MailboxTable[mboxIndex];  // Create Mailbox pointer

    /*** Error Check: Invalid Parameters ***/
    //check mailbox id
    if(mbox_id < 0){
        DebugConsole2("%s : Illegal mailbox ID [%d].\n",
                      __func__, mbox_id);
        return -1;
    }

    //check msg_ptr
    if(msg_ptr == NULL){
        DebugConsole2("%s : Invalid message pointer.\n",
                      __func__);
        return -1;
    }

    //check msg_size
    if(msg_size > MAXMESSAGE || msg_size < 0){
        DebugConsole2("%s : Invalid message size [%d].\n",
                      __func__, msg_size);
        return -1;
    }

    /*** Verify Slot is Available ***/
    if(pMbox->activeSlots >= pMbox->maxSlots){                       //if exceeding max slots
        AddProcessLL(getpid(),&pMbox->waitingToSend);      //add proc to waiting list
        block_me(SEND_BLOCK);                                       //block sending process

    }

    /*** Add Message and Slot into Mailbox ***/
    if((AddSlotLL(msg_ptr, msg_size, pMbox)) == -1){
        DebugConsole2("%s: Invalid Send Unblocking - Mailbox Full. Halting...", __func__);
        halt(1);
    }

    /*** Unblock Waiting Processes ***/
    if(!ListIsEmpty(NULL, &pMbox->waitingToReceive)){  //if list is not empty
        int pid = RemoveProcessLL(pMbox->waitingToReceive.pHeadProc->pid,
                                  &pMbox->waitingToReceive);    //remove from list
        unblock_proc(pid);                                          //unlock process
    }

    // Enable interrupts
    enableInterrupts();

    /*** Error Check: Mailbox was Released ***/   //todo release vs empty ?
    if(pMbox->status == MBOX_EMPTY){
        DebugConsole2("%s: mailbox was released while process was blocked.",
                      __func__);
        return -3;
    }

    /*** Error Check: Process was Zapped ***/
    if(is_zapped()){
        DebugConsole2("%s: Process was zapped while it was blocked",
                      __func__);
        return -3;
    }


    return 0;
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name -           MboxReceive
   Purpose -        Receive a message from a mailbox
   Parameters -     mbox_id:        Unique Mailbox ID
                    msg_ptr:        Pointer to the Message
                    max_msg_size:   Max Storage Size for Message in Bytes
   Returns -        -3: process is zapped,
                        mailbox was released while the process
                        was blocked on the mailbox.
                    -1: Invalid Parameters Given,
                        Sent Message Size Exceeds Limit
                    >0: Size of Message Received
   Side Effects -   none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int max_msg_size)
{
    //NOTE:
    // messages are received in the order the receive function is called.

    /*** Function Initialization ***/
    check_kernel_mode(__func__);    // Check for kernel mode
    disableInterrupts();                        // Disable interrupts
    int mboxIndex = GetMboxIndex(mbox_id);  //Get Mailbox index in MailboxTable
    mailbox *pMbox = &MailboxTable[mboxIndex];  // Create Mailbox pointer

    /*** Error Check: Invalid Parameters ***/
    //check if mailbox id is invalid
    if(mbox_id < 0){
        DebugConsole2("%s : Illegal mailbox ID [%d].\n",
                      __func__, mbox_id);
        return -1;
    }
    if(pMbox->mbox_id != mbox_id){
        DebugConsole2("%s : Process ID [%d] can not be found.\n",
                      __func__, mbox_id);
        return -1;
    }

    //check if msg_ptr is invalid
    if(msg_ptr == NULL){
        DebugConsole2("%s : Invalid message pointer.\n",
                      __func__);
        return -1;
    }

    //check if msg_size is invalid
    if(max_msg_size > MAXMESSAGE || max_msg_size < 0){
        DebugConsole2("%s : Invalid maximum message size [%d].\n",
                      __func__, max_msg_size);
        return -1;
    }

    /*** Verify Message is Available ***/
    if(pMbox->activeSlots <= 0){                                    //if no message available
        AddProcessLL(getpid(),&pMbox->waitingToReceive);   //add proc to waiting list
        block_me(RECEIVE_BLOCK);                                    //block receiving process

        if(pMbox->slotQueue.pHeadSlot == NULL){     //Verify Unblocked when Message Available
            DebugConsole2("%s: Invalid Receive Unblocking - Mailbox Empty. Halting...", __func__);
            halt(1);
        }
    }

    /*** Error Check: Sent Message Size Exceeds Limit ***/
    int sentMsgSize = pMbox->slotQueue.pHeadSlot->slot->messageSize;
    if(sentMsgSize > max_msg_size){
        DebugConsole2("%s : Sent message size [%d] exceeds maximum receive size [%d] .\n",
                      __func__, sentMsgSize, max_msg_size);
        return -1;
    }

    /*** Retrieve Message ***/
    //copy message into buffer
    memcpy(msg_ptr, pMbox->slotQueue.pHeadSlot->slot->message, sentMsgSize);

    /*** Remove Slot/Message from Queue ***/
    if((RemoveSlotLL(&pMbox->slotQueue)) == -1){
        DebugConsole2("%s: Unable to receive message - mailbox empty. Halting...",
                      __func__);
        halt(1);
    }

    /*** Unblock Waiting Processes ***/
    if(!ListIsEmpty(NULL, &pMbox->waitingToSend)){  //if list is not empty
        int pid = RemoveProcessLL(pMbox->waitingToSend.pHeadProc->pid,
                                  &pMbox->waitingToSend);    //remove from list
        unblock_proc(pid);                                       //unlock process
    }

    /*** Error Check: Process was Zapped ***/
    if(is_zapped()){
        DebugConsole2("%s: Process was zapped while it was blocked",
                      __func__);
        return -3;
    }

    /*** Error Check: Mailbox was Released ***/
    if(pMbox->status == MBOX_EMPTY){
        DebugConsole2("%s: Mailbox was released while process was blocked",
                      __func__);
        return -3;
    }

    // Enable interrupts
    enableInterrupts();

    //Return size of message received
    return sentMsgSize;

} /* MboxReceive */


/* ------------------------------------------------------------------------
   Name -           MboxRelease
   Purpose -        Releases a previously created mailbox
   Parameters -     mbox_id:  Unique Mailbox ID
   Returns -        -3:  Process was Zapped while Releasing the Mailbox
                    -1:  Invalid Mailbox ID
                     0:  Success
   Side Effects -
   ----------------------------------------------------------------------- */
int MboxRelease(int mbox_id)
{
    /*** Function Initialization ***/
    check_kernel_mode(__func__);    // Check for kernel mode
    disableInterrupts();                        // Disable interrupts
    int mboxIndex = GetMboxIndex(mbox_id);  //Get Mailbox index in MailboxTable
    mailbox *pMbox = &MailboxTable[mboxIndex];  // Create Mailbox pointer


    /*** Error Check: Invalid Parameters ***/
    //check if mailbox id is invalid
    if(mbox_id < 0){
        DebugConsole2("%s : Illegal mailbox ID [%d].\n",
                      __func__, mbox_id);
        return -1;
    }
    else if(pMbox->mbox_id != mbox_id){
        DebugConsole2("%s : Process ID [%d] can not be found.\n",
                      __func__, mbox_id);
        return -1;
    }

    /*** Error Check: Unused Mailbox ***/
    if(pMbox->status == MBOX_EMPTY){
        DebugConsole2("%s: Attempting to release an already empty slot.",
                      __func__);
        return -1;
    }

    /*** Clear out Mailbox Slot ***/
    HelperRelease(pMbox);

    /*** Error Check: Process was Zapped ***/
    if(is_zapped()){
        DebugConsole2("%s: Process was zapped while it was blocked",
                      __func__);
        return -3;
    }

    // Enable interrupts
    enableInterrupts();

    //Successful Release
    return 0;

} /* MboxRelease */

/* ------------------------------------------------------------------------
 * TODO: Send Message, Combine with Helper Send, Finish Error Checks
   Name -           MboxCondSend
   Purpose -        Send a message to a mailbox
   Parameters -     mailboxID:      Unique Mailbox ID
                    *message:       Pointer to the Message
                    message_size:   Size of Message in Bytes
   Returns -        -3: Process is Zapped,
                    -2: Mailbox is Full, Message not Sent;
                        No Mailbox Slots Available in System.
                    -1: Invalid Parameters Given,
                     0: Message Sent Successfully.
   Side Effects -   none
   ----------------------------------------------------------------------- */
int MboxCondSend(int mailboxID, void *message, int message_size)
{
    /*** Function Initialization ***/
    check_kernel_mode(__func__);    // Check for kernel mode
    disableInterrupts();                        // Disable interrupts
    int mboxIndex = GetMboxIndex(mailboxID);  //Get Mailbox index in MailboxTable
    mailbox *pMbox = &MailboxTable[mboxIndex];  // Create Mailbox pointer

    /*** Error Check: Invalid Parameters ***/
    //check mailbox id
    if(mailboxID < 0){
        DebugConsole2("%s : Illegal mailbox ID [%d].\n",
                      __func__, mailboxID);
        return -1;
    }

    //check msg_ptr
    if(message == NULL){
        DebugConsole2("%s : Invalid message pointer.\n",
                      __func__);
        return -1;
    }

    //check msg_size
    if(message_size > MAXMESSAGE || message_size < 0){
        DebugConsole2("%s : Invalid message size [%d].\n",
                      __func__, message_size);
        return -1;
    }


    //SEND MESSAGE

    /*** Error Check: No Available Slots ***/
    //TODO return -2

    /*** Error Check: Mailbox Full/ Message not Sent ***/
    //TODO return -2

    /*** Error Check: Process was Zapped ***/
    if(is_zapped()){
        DebugConsole2("%s: Process was zapped while it was blocked",
                      __func__);
        return -3;
    }


    // Enable interrupts
    enableInterrupts();

    //Message Sent Successfully
    return 0;
} /* MboxCondSend */


/* ------------------------------------------------------------------------
 * TODO
   Name -           MboxCondReceive
   Purpose -
   Parameters -     mailboxID:      Unique Mailbox ID
                    *message:       Pointer to the Message
                    max_msg_size:   Max Storage Size for Message in Bytes
   Returns -        -3: Process is Zapped,
                    -2: Mailbox is Full, Message not Sent;
                        No Mailbox Slots Available in System.
                    -1: Invalid Parameters Given,
                     0: Message Sent Successfully.
   Side Effects -   none
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mailboxID, void *message, int max_msg_size)
{
    /* Return values:
    -3:  process is zapped.
    -2:  mailbox empty, no message to receive.
    -1:  illegal values given as arguments; or, message sent is too large for
    receiver's buffer (no data copied in this case).
    >= 0:  the size of the message received.
    */

    /*** Function Initialization ***/
    check_kernel_mode(__func__);    // Check for kernel mode
    disableInterrupts();                        // Disable interrupts
    int mboxIndex = GetMboxIndex(mailboxID);  //Get Mailbox index in MailboxTable
    mailbox *pMbox = &MailboxTable[mboxIndex];  // Create Mailbox pointer

} /* MboxCondReceive */

/* ------------------------------------------------------------------------
 * TODO
   Name -           check_io
   Purpose -
   Parameters -     none
   Returns -        -1:
                     0:
   Side Effects -   none
   ----------------------------------------------------------------------- */
int check_io()
{
    return 0;
} /* check_io */

/* ------------------------------------------------------------------------
 * TODO
   Name -           waitdevice
   Purpose -
   Parameters -     type:
                    unit:
                    status:
   Returns -        -1: the process was zapped while waiting
                     0: otherwise
   Side Effects -   none
   ----------------------------------------------------------------------- */
int waitdevice(int type, int unit, int *status)
{
    /*** Function Initialization ***/
    check_kernel_mode(__func__);    // Check for kernel mode

}  /* waitdevice */

/* ------------------------------------------------------------------------
 * TODO
   Name -           HelperSend
   Purpose -
   Parameters -     mbox_index:      MailboxTable Index
                    *msg:           Pointer to the Message
                    msg_size:       Size of Message in Bytes
                    block_flag:     (1) Blocked, (0) Unblocked
   Returns -        none
   Side Effects -   none
   ----------------------------------------------------------------------- */
void HelperSend(int mbox_index, void *msg, int msg_size, int block_flag)
{
    // fill me

    //if message already there, there are no differences between functions
    //block_flag - should proc wait or ?
    //if no block, return immediately?
    return;
}


/* ------------------------------------------------------------------------
 * TODO
   Name -           HelperReceive
   Purpose -
   Parameters -     mbox_index:      MailboxTable Index
                    *msg:           Pointer to the Message
                    msg_size:       Size of Message in Bytes
                    block_flag:     (1) Blocked, (0) Unblocked
   Returns -        none
   Side Effects -   none
   ----------------------------------------------------------------------- */
void HelperReceive(int mbox_index, void *msg, int msg_size, int block_flag)
{
    // fill me
    return;
}

/* ------------------------------------------------------------------------
   Name -           enableInterrupts
   Purpose -        Enable System Interrupts
   Parameters -     none
   Returns -        none
   Side Effects -   Modifies PSR Values
   ----------------------------------------------------------------------- */
static void enableInterrupts()
{
    check_kernel_mode(__func__);
    /*Confirmed Kernel Mode*/

    int curPsr = psr_get();

    curPsr = curPsr | PSR_CURRENT_INT;

    psr_set(curPsr);
} /* enableInterrupts */


/* ------------------------------------------------------------------------
   Name -           disableInterrupts
   Purpose -        Disable System Interrupts
   Parameters -     none
   Returns -        none
   Side Effects -   Modifies PSR Values
   ----------------------------------------------------------------------- */
static void disableInterrupts()
{
    /* turn the interrupts OFF if we are in kernel mode */
    if((PSR_CURRENT_MODE & psr_get()) == 0) {
        //not in kernel mode
        console("Kernel Error: Not in kernel mode, may not disable interrupts. Halting...\n");
        halt(1);
    } else
        /* We ARE in kernel mode */
        psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */

/* ------------------------------------------------------------------------
   Name -           check_kernel_mode
   Purpose -        Checks the PSR for kernel mode and halts if in user mode
   Parameters -     functionName:   Function to Verify
   Returns -        none
   Side Effects -   Halts if not kernel mode
   ----------------------------------------------------------------------- */
static void check_kernel_mode(const char *functionName)
{
//    union psr_values psrValue; /* holds caller's psr values */
//
//    DebugConsole2("check_kernel_node(): verifying kernel mode for %s\n", functionName);
//
//    /* test if in kernel mode; halt if in user mode */
//    psrValue.integer_part = psr_get();  //returns int value of psr
//
//    if (psrValue.bits.cur_mode == 0)
//    {
//        if (Current) {  //if currently running function
//            int cur_proc_slot = Current->pid % MAXMBOX;
//            console("%s(): called while in user mode, by process %d. Halting...\n", functionName, cur_proc_slot);
//        }
//        else {  //if no currently running function, startup is running
//            console("%s(): called while in user mode, by startup(). Halting...\n", functionName);
//        }
//        halt(1);
//    }

    DebugConsole2("Function is in Kernel mode (:\n");
} /* check_kernel_mode */


/* ------------------------------------------------------------------------
   Name -           DebugConsole2
   Purpose -        Prints the message to the console if in debug mode
   Parameters -     *format:    Format String to Display
                    ...:        Optional Corresponding Arguments
   Returns -        none
   Side Effects -   none
   ----------------------------------------------------------------------- */
void DebugConsole2(char *format, ...)
{
    if (DEBUG2 && debugFlag2)
    {
        va_list argptr;
        va_start(argptr, format);
        console(format, argptr);
        va_end(argptr);
    }
}

/* ------------------------------------------------------------------------
 * TODO: delete? check if used
 *
    Name -          CopyToList
    Purpose -       Copies process at MailBoxTable[mbox_index] to pStat
    Parameters -    mbox_index: location of mailbox in MailBoxTable
                    pSlot: Address of mailbox slot
    Returns -       none
    Side Effects -  none
   ----------------------------------------------------------------------- */
static void CopyToList(int mbox_index, SlotList * pSlot)
{
    //pStat->process = &ProcTable[proc_slot];
    return;
}

/* ------------------------------------------------------------------------
 * TODO
 *
    Name -          time_slice
    Purpose -       calls the dispatcher if the currently executing process
                        has exceeded its time slice
    Parameters -    none
    Returns -       none
    Side Effects -  none
   ----------------------------------------------------------------------- */
void Time_Slice(void){
//    int proc_slot;
//    int curPid;
//    int currentTime = sys_clock();
//
//    Current->totalCpuTime += ReadTime(); //calculate total cpu time
//    curPid = getpid();                    //set current pid
//    proc_slot = (curPid) % MAXMBOX;     //find current proc_slot
//
//
//    if (Current->status == STATUS_RUNNING) {  //if current status is running (not blocked)
//        if ((currentTime - ((Current->switchTime) / 1000)) > TIME_SLICE) {  //if exceeded
//
//            AddProcessLL(proc_slot, &ReadyList);
//            Current->status = STATUS_READY; //switch Current status to ready
//            dispatcher();
//        }
//    }
//
//    return;
}


/* ------------------------------------------------------------------------
 * TODO
 *
    Name -          InList
    Purpose -       iterates through list pStat to find process using pid
    Parameters -    id:     Unique Parameter ID [mbox_id, slot_id, pid]
                    list:   List to Search Through [slot, mbox, proc]
    Returns -       0:  not found
                    1:  found
    Side Effects -  none
   ----------------------------------------------------------------------- */
bool InList(int id, char list) {
//    int proc_slot = pid % MAXMBOX;                  //find zapped_proc_slot
//    proc_ptr Process = &ProcTable[proc_slot];       //pointer to zapped process
//    StatusStruct * pSave;                           //used to iterate through ReadyList
//
//    int index = (Process->priority)-1;   //index in list
//    pSave = pStat[index].pHeadProc;     //Find correct index, Start at head
//
//    if(pSave == NULL){  //if pSave is NULL, process not found
//        return 0;
//    }
//
//    //iterate through list
//    while(pSave->process != NULL){          //verify not end of list
//
//        if(pSave->process->pid == pid){     //if PIDs match,
//            return 1;                       //process found
//        }
//        else{
//            pSave = pSave->pNextProc;       //iterate to next on list
//        }
//    }
//    return 0;   //not found
}

/* * * * * * * * * * * * Mailbox Table Functions * * * * * * * * * * * */

/* ------------------------------------------------------------------------
   Name -           GetNextMailID
   Purpose -        Finds next mbox_id
   Parameters -     none
   Returns -        >0: next mbox_id
   Side Effects -   none
   ----------------------------------------------------------------------- */
int GetNextMailID() {
    int newMid = -1;
    int mboxSlot = next_mid % MAXMBOX;

    if (totalMailbox < MAXMBOX) {
        while ((totalMailbox < MAXMBOX) && (MailboxTable[mboxSlot].status != MBOX_EMPTY)) {
            next_mid++;
            mboxSlot = next_mid % MAXMBOX;
        }

        newMid = next_mid;    //assign newPid *then* increment next_pid
    }

    return newMid;
}

/* ------------------------------------------------------------------------
   Name -           InitializeMailbox
   Purpose -        Initializes the new mailbox
   Parameters -     mbox_index:  Location inside MailBoxTable
                    mbox_id:    Unique Mailbox ID
                    maxSlots:   Maximum number of slots
                    slotSize:   Maximum size of slot/ message
   Returns -        none
   Side Effects -   Mailbox added to MailboxTable
   ----------------------------------------------------------------------- */
void InitializeMailbox(int newStatus, int mbox_index, int mbox_id, int maxSlots, int slotSize) {

    //if removing a mailbox from MailboxTable
    if(newStatus == MBOX_EMPTY) {
        mbox_id = -1;
        maxSlots = -1;
        slotSize = -1;
    }


    MailboxTable[mbox_index].mbox_index = mbox_index;                  //Mailbox Slot
    MailboxTable[mbox_index].mbox_id = mbox_id;                      //Mailbox ID
    MailboxTable[mbox_index].status = newStatus;                        //Mailbox Status
    InitializeList(NULL, &MailboxTable[mbox_index].waitingProcs);    //Waiting Process List
    InitializeList(NULL, &MailboxTable[mbox_index].waitingToSend);   //Waiting to Send List
    InitializeList(NULL, &MailboxTable[mbox_index].waitingToReceive);//Waiting to Receive List
    MailboxTable[mbox_index].maxSlots = maxSlots;                    //Max Slots
    MailboxTable[mbox_index].activeSlots = 0;                        //Active Slots
    MailboxTable[mbox_index].slotSize = slotSize;                    //Slot Size
    InitializeList(&MailboxTable[mbox_index].slotQueue, NULL);       //Slot List
}
/* ------------------------------------------------------------------------
   Name -           GetMboxIndex
   Purpose -        Gets Mailbox index from id
   Parameters -     mbox_id:    Unique Mailbox ID
   Returns -        none
   Side Effects -   none
   ----------------------------------------------------------------------- */
int GetMboxIndex(int mbox_id){
    return mbox_id % MAXMBOX;
}
/* * * * * * * * * * * END OF Mailbox Table Functions * * * * * * * * * * */



/* * * * * * * * * * * * * Linked List Functions * * * * * * * * * * * * */

/* ------------------------------------------------------------------------
    Name -          InitializeList
    Purpose -       Initialize List pointed to by parameter
    Parameters -    Meant to be used as 'either or', use NULL for other
                    *pSlot: Pointer to Message Slot List
                    *pProc: Pointer to Process List
    Returns -       None, halt(1) on error
    Side Effects -  None
   ----------------------------------------------------------------------- */
void InitializeList(slotQueue *pSlot, procQueue *pProc)
{
    /*** Initialize Slot Queue ***/
    if(pProc == NULL) {
        /* Initialize the head, tail, and count slot queue */
        pSlot->pHeadSlot = NULL;
        pSlot->pTailSlot = NULL;
        pSlot->total = 0;
        return;
    }

    /*** Initialize Proc Queue ***/
    else if(pSlot == NULL){
        /* Initialize the head, tail, and count proc queue */
        pProc->pHeadProc = NULL;
        pProc->pTailProc = NULL;
        pProc->total = 0;
    }

    /*** Error Check: Invalid Parameters ***/
    else{
        DebugConsole2("%s: Invalid Parameters Passed", __func__);
        halt(1);
    }
}

/* ------------------------------------------------------------------------
    Name -          ListIsFull
    Purpose -       Checks if Slot List OR Proc List is Full
    Parameters -    Meant to be used as 'either or', use NULL for other
                    *pSlot: Pointer to Message Slot List
                    *pProc: Pointer to Process List
    Returns -        0: not full
                     1: full
                    -1: list not found
    Side Effects -  none
   ----------------------------------------------------------------------- */
bool ListIsFull(const slotQueue *pSlot, const procQueue *pProc){

    /*** Search if Slot Queue is Full ***/
    if(pProc == NULL) {
        return pSlot->total >= MAXSLOTS;
    }

    /*** Search if Proc Queue is Full ***/
    else if(pSlot == NULL){
        return pProc->total >= MAXPROC;
    }

    /*** Error Check: Invalid Parameters ***/
    else{
        DebugConsole2("%s: Invalid Parameters Passed", __func__);
        halt(1);
    }
}


/* ------------------------------------------------------------------------
    Name -          ListIsEmpty
    Purpose -       Checks if Slot List OR Proc List is Empty
    Parameters -    Meant to be used as 'either or', use NULL for other
                    *pSlot: Pointer to Message Slot List
                    *pProc: Pointer to Process List
    Returns -        0: not Empty
                     1: Empty
                    -1: list not found
    Side Effects -  none
   ----------------------------------------------------------------------- */
bool ListIsEmpty(const slotQueue *pSlot, const procQueue *pProc){

    /*** Search if Slot Queue is Empty ***/
    if(pProc == NULL) {
        return pSlot->pHeadSlot == NULL;    //checks if pHead is empty
    }

    /*** Search if Proc Queue is Empty ***/
    else if(pSlot == NULL){
        return pProc->pHeadProc == NULL;    //checks if pHead is empty
    }

    /*** Error Check: Invalid Parameters ***/
    else{
        DebugConsole2("%s: Invalid Parameters Passed", __func__);
        halt(1);
    }
}


/* ------------------------------------------------------------------------
    Name -          AddSlotLL
    Purpose -       Adds Slot with Message to slotQueue
    Parameters -    slotIndex:  Slot Index in Mailbox
                    mbox:       Pointer to Mailbox
    Returns -        0: Success
                    -1: Slot Queue is Full
    Side Effects -  None
   ----------------------------------------------------------------------- */
int AddSlotLL(void *msg_ptr, int msg_size, mailbox * mbox){

    SlotList * pList;     //new slot list
    slot_ptr pSlot;       //new slot node
    int index = GetMboxIndex(mbox->mbox_id);   //get mailbox index

    /*** Error Check: Verify Slot Space Available***/
    // check if slot queue is full
    if(ListIsFull(&mbox->slotQueue, NULL)){
        DebugConsole2("%s: Slot Queue is full.\n", __func__);
        return -1;
    }
    // check if exceeding active slots
    if (totalActiveSlots >= MAXSLOTS){
        DebugConsole2("%s: No Active Slots Available. Halting...\n", __func__);
        halt(1);
    }

    /*** Prepare pList and pNode to add to Linked List ***/
    //allocate pList
    pList = (SlotList *) malloc (sizeof (SlotList));
    pSlot = (Slot_Table *) malloc (sizeof (Slot_Table));

    //verify allocation
    if (pList == NULL || pSlot == NULL){
        DebugConsole2("%s: Failed to allocate memory for node in linked list. Halting...\n",
                      __func__ );
        halt(1);    //memory allocation failed
    }

    /*** Combine Message, Node, and List ***/
    memcpy(&pSlot->message,msg_ptr, msg_size );   //copy message to pSlot
    pSlot->messageSize = msg_size;  //update message size
    pSlot->status = SLOT_FULL;      //update pSlot status
    pSlot->mbox_id = mbox->mbox_id; //update pSlot Mailbox ID
    pList->slot = pSlot;            //add pSlot to pList

    /*** Add pList to Queue and Update Links ***/
    pList->pNextSlot = NULL;                            //Add pList to slotQueue

    if(ListIsEmpty(&mbox->slotQueue, NULL)){    //if index is empty...
        mbox->slotQueue.pHeadSlot = pList;                  //add to front of list
    }
    else{
        mbox->slotQueue.pTailSlot->pNextSlot = pList;    //add to end of list'
        pList->pPrevSlot = mbox->slotQueue.pTailSlot;    //assign pPrev to current Tail
    }

    mbox->slotQueue.pTailSlot = pList;                   //reassign pTail to new Tail

    /*** Update Counters***/
    totalSlots++;               //global: total slots
    totalActiveSlots++;         //global: total active slots
    mbox->activeSlots++;        //mailbox active slots
    mbox->slotQueue.total++;    //mailbox total
    mbox->slotQueue.mbox_id = mbox->mbox_id;    //update slotQueue Mailbox ID

    return 0;
}

/* ------------------------------------------------------------------------
    Name -          RemoveSlotLL
    Purpose -       Remove Slot from Specific Mailbox
    Parameters -    pSlotQ:  Pointer to Queue to remove Slot from
    Returns -       None
    Side Effects -  None
   ----------------------------------------------------------------------- */
int RemoveSlotLL(slotQueue * pSlotQ){
    SlotList * pSave;                       //New node to save position
    pSave = pSlotQ->pHeadSlot;              //Save slot pointer to list head
    int mbox_index = GetMboxIndex(pSlotQ->mbox_id);  //Get Mailbox index in MailboxTable
    mailbox *pMbox = &MailboxTable[mbox_index];              //Create Mailbox pointer

    /*** Error Check: Verify Messages in List ***/
    if(pSave == NULL){  //Check that pSave returned process
        DebugConsole2("%s: Attempting to Remove Slot from Empty List\n", __func__);
        return -1;
    }

    /*** Remove Slot and Update Linkers ***/
    pSlotQ->pHeadSlot = pSave->pNextSlot;           //set next slot as head
    pSave->pNextSlot = NULL;                        //set old head->next to NULL

    if(pSlotQ->pHeadSlot != NULL){                  //if NOT only node in Queue
        pSlotQ->pHeadSlot->pPrevSlot = NULL;        //set new head->prev to NULL
    }
    else{                                           //if only node in Queue
        pSave->pPrevSlot = NULL;                    //clear linkers
        pSlotQ->pTailSlot = NULL;
    }

    /*Note:
     * not decrementing totalSlots
     * because it is still valid,
     * it is just empty*/

    /*** Decrement Counters ***/
    pMbox->activeSlots--;
    totalActiveSlots--;

    return 0;
}

/* ------------------------------------------------------------------------
    Name -          AddProcessLL
    Purpose -       Add Process to Specified Process Queue
    Parameters -    pid:    Process ID
                    pq:     Pointer to Process Queue
    Returns -       None, halt (1) on error
    Side Effects -  None
   ----------------------------------------------------------------------- */
void AddProcessLL(int pid, procQueue * pq){

    /*** Initialize Variable(s) ***/
    ProcList * pNew;    //New Node

    /*** Error Check: List is Full ***/
    if(ListIsFull(NULL, pq)){
        DebugConsole2("%s: Queue is full.\n", __func__);
        halt(1);
    }

    /*** Allocate Space for New Node ***/
    pNew = (ProcList *) malloc (sizeof (ProcList));

    /* Verify Allocation Success */
    if(pNew == NULL){
        DebugConsole2("%s: Failed to allocate memory for node in linked list. Halting...\n", __func__);
        halt(1);    //memory allocation failed
    }

    /*** Add New Node to List ***/
    pNew->pid = pid;                            //update pid value
    pNew->pNextProc = NULL;                     //assign pNext to NULL

    if(ListIsEmpty(NULL, pq)){      //if list is empty...
        pq->pHeadProc = pNew;                   //add pNew to front of list
    }
    else{                                       //if list not empty...
        pq->pTailProc->pNextProc = pNew;        //add pNew to end of list
        pNew->pPrevProc = pq->pTailProc;        //assign pNew prev to current tail
    }

    pq->pTailProc = pNew;                       //reassign pTail to new node
    pq->total++;                                //update process queue counter

    /*** List Addition Success ***/
    return;
}

/* ------------------------------------------------------------------------
    Name -          RemoveProcessLL
    Purpose -       Removes Process from Specified Process Queue
    Parameters -    pid:    Process ID
                    pq:     Pointer to Process Queue
    Returns -       Pid of removed Process, halt (1) on error
    Side Effects -  None
   ----------------------------------------------------------------------- */
int RemoveProcessLL(int pid, procQueue * pq){

    /*** Initialize Variable(s) ***/
    ProcList * pSave;   //Pointer for Linked List
    int pidFlag = 0;    //verify pid found in list
    int remFlag = 0;    //verify process removed successfully

    /*** Error Check: List is Empty ***/
    if(ListIsEmpty(NULL, pq)){
        DebugConsole2("%s: Queue is empty.\n", __func__);
        halt(1);
    }

    /*** Find PID in List ***/
    pSave = pq->pHeadProc;          //Set pSave to head of list

    while(pSave->pid != 0) {        //verify not end of list    //todo: does 0 work? x>0?
        if(pSave->pid == pid){      //if found pid to remove...
            pidFlag = 1;            //Trigger Flag - PID found

            /** If pid to be removed is at head **/
            if(pSave == pq->pHeadProc){
                pq->pHeadProc = pSave->pNextProc;           //Set next pid as head
                pSave->pNextProc = NULL;                    //Set old head next pointer to NULL

                /* if proc is NOT only node on list */
                if(pq->pHeadProc != NULL) {
                    pq->pHeadProc->pPrevProc = NULL;        //Set new head prev pointer to NULL
                }

                /* if proc IS only node on list */
                else{
                    pSave->pPrevProc = NULL;    //Set old prev pointer to NULL
                    pq->pTailProc = NULL;       //Set new tail to NULL

                }

                remFlag = 1;                    //Trigger Flag - Process Removed
                break;
            }

            /** If pid to be removed is at tail **/
            else if(pSave == pq->pTailProc){
                pq->pTailProc = pSave->pPrevProc;       //Set prev pid as tail
                pSave->pPrevProc = NULL;                //Set old tail prev pointer to NULL
                pq->pTailProc->pNextProc = NULL;        //Set new tail next pointer to NULL
                remFlag = 1;                            //Trigger Flag - Process Removed
                break;
            }

            /** If pid to be removed is in middle **/
            else{
                pSave->pPrevProc->pNextProc = pSave->pNextProc; //Set pSave prev next to pSave next
                pSave->pNextProc->pPrevProc = pSave->pPrevProc; //Set pSave next prev to pSave prev
                pSave->pNextProc = NULL;    //Set old pNext to NULL
                pSave->pPrevProc = NULL;    //Set old pPrev to NULL
                remFlag = 1;                //Trigger Flag - Process Removed
                break;
            }
        }
        else{                           //if pid not found
            pSave = pSave->pNextProc;   //increment to next in list
        }
    }//end while

    /*** Decrement Counter ***/
    pq->total--;

    /*** Error Check: Pid Found and Removed ***/
    if(pidFlag && remFlag){
        pSave = NULL;       //free() causing issues in USLOSS

    }
    else {
        if (pidFlag == 0){
            DebugConsole2("%s: Unable to locate pid [%d]. Halting...\n",
                          __func__, pid);
            halt(1);
        }
        else if (remFlag == 0) {
            DebugConsole2("%s: Removal of pid [%d] unsuccessful. Halting...\n",
                          __func__, remFlag);
            halt(1);
        }
    }

    return pid;
}

/* * * * * * * * * * * * END OF Linked List Functions * * * * * * * * * * * */



/* * * * * * * * * * * Mailbox Slots Release Functions * * * * * * * * * * */

/* ------------------------------------------------------------------------
    Name -          clearMboxSlot
    Purpose -       Clears out Mailbox Slot
    Parameters -    mbox: Pointer to Mailbox
    Returns -       none
    Side Effects -  none
   ----------------------------------------------------------------------- */
void HelperRelease(mailbox *mbox) {

    ProcList * pSaveProc;
    /*** Update Global Counters ***/
    totalSlots -= mbox->maxSlots;           //decrement total
    totalActiveSlots -= mbox->activeSlots;  //decrement active
    totalMailbox--;                         //decrement mailbox

    /*** Handle Mailbox Processes ***/
    /* Check Waiting Processes */
    mbox->waitingProcs.pHeadProc != NULL ? UnblockProcs(mbox->waitingProcs.pHeadProc): NULL;

    /* Check Waiting to Send Processes */
    mbox->waitingToSend.pHeadProc != NULL ? UnblockProcs(mbox->waitingToSend.pHeadProc) : NULL;

    /* Check Waiting to Receive Processes */
    mbox->waitingToReceive.pHeadProc != NULL ? UnblockProcs( mbox->waitingToReceive.pHeadProc) : NULL;

    /*** Clear Mailbox Values ***/
    InitializeMailbox(MBOX_EMPTY, mbox->mbox_index, NULL, NULL, NULL);

}

/* ------------------------------------------------------------------------
    Name -          UnblockProcs
    Purpose -       Handles blocked processes on a released mailbox
    Parameters -    pSaveProc: Pointer to Save
    Returns -       none
    Side Effects -  none
   ----------------------------------------------------------------------- */
void UnblockProcs(ProcList *pSaveProc)
{
    /* Unblock Found Processes */
    while(pSaveProc != NULL){           //while not end of list
        int pid = pSaveProc->pid;       //save pid
        unblock_proc(pid);              //unblock pid
        pSaveProc = pSaveProc->pNextProc;   //increment pid
    }
}

/* * * * * * * * * * END OF Mailbox Slots Release Functions * * * * * * * * * */