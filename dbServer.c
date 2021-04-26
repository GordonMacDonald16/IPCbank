#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <signal.h>

#define PERMS 0644 
#define DB_PATH "db.txt"
#define MAX_ARGUMENTS 3
#define DB_CAPACITY 25
#define MAX_ATM_INSTANCES 10
#define MAX_FIELD_LENGTH 10
#define ATM_QUEUE "msgq-atm.txt"
#define IPC_ERROR -1
#define DB_SEM_FNAME "db-semaphore.txt"


volatile int dbSize;
volatile int badAttempts;
struct account_info *currentAccount = NULL;
int s_mutex;
int subscribedAtms[MAX_ATM_INSTANCES];

enum transaction_type{
    UPDATE_DB = 0,
    PIN = 1,
    BALANCE = 2,
    WITHDRAW = 3,
    KILL = 4,
    LOCK = 5,
    UNLOCK = 6
};

struct my_msgbuf
{
    long mtype;
    char mtext[200];
};


struct account_info{
    char acc_num[MAX_FIELD_LENGTH];
    int pin;
    float balance;
    int failedAccessAttempts;
};

struct transaction
{
    enum transaction_type action;
    struct account_info account;
    int value;
};

enum reply_type{
    PIN_WRONG = 0,
    PIN_OK = 1,
    BALANCE_REPLY = 2,
    NSF = 3,
    FUNDS_OK = 4
};

int SemaphoreWait(int semid, int iMayBlock)
{
    struct sembuf sbOperation;
    sbOperation.sem_num = 0;
    sbOperation.sem_op = -1;
    sbOperation.sem_flg = iMayBlock;
    return semop(semid, &sbOperation, 1);
}
int SemaphoreSignal(int semid)
{
    struct sembuf sbOperation;
    sbOperation.sem_num = 0;
    sbOperation.sem_op = +1;
    sbOperation.sem_flg = 0;
    return semop(semid, &sbOperation, 1);
}

void SemaphoreRemove(int semid)
{
    if (semid != IPC_ERROR)
        semctl(semid, 0, IPC_RMID, 0);
}

int SemaphoreCreate(int iInitialValue)
{
    int semid;
    //union semun suInitData;

    key_t key;

    if ((key = ftok(DB_SEM_FNAME, 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }
    //int iError;
    /* get a semaphore */
    semid = semget(key, 1, PERMS | IPC_CREAT);
    /* check for errors */
    if (semid == IPC_ERROR)
        return semid;
    /* now initialize the semaphore */
    //suInitData.val = iInitialValue;
    if (semctl(semid, 0, SETVAL, iInitialValue) == IPC_ERROR)
    { /* error occurred, so remove semaphore */
        SemaphoreRemove(semid);
        return IPC_ERROR;
    }
    return semid;
}

void replyToAtm(enum reply_type reply, char *balance){
    struct my_msgbuf buf;
    int msqid;
    int len;
    key_t key;

    if ((key = ftok(ATM_QUEUE, 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1)
    {
        perror("msgget");
        exit(1);
    }

    buf.mtype = 1; /* we don't really care in this case */


    switch (reply)
    {
    case PIN_WRONG:
        strcpy(buf.mtext, "PIN_WRONG");
        break;
    case PIN_OK:
        strcpy(buf.mtext, "PIN_OK");
        break;
    case NSF:
        strcpy(buf.mtext, "NSF");
        break;
    case BALANCE:{
        strcat(strcpy(buf.mtext, "BALANCE,"), balance);
 
        break;}
    case FUNDS_OK:
        strcat(strcpy(buf.mtext, "FUNDS_OK,"), balance);
        break;
    default:
        break;
    }

    len = strlen(buf.mtext);
    if (msgsnd(msqid, &buf, len + 1, 0) == -1) /* +1 for '\0' */
        perror("msgsnd");

}


//Refresh the accounts array by allocating space for the current acocunts
//return pointer to the first account
//**************PUSH MUST FREE THE POINTER THAT IS RETURNED**********
struct account_info *pullFromDb(){
    dbSize = 0;

    char line[DB_CAPACITY][MAX_FIELD_LENGTH * MAX_ARGUMENTS];
    FILE *db = fopen(DB_PATH, "r");
    struct account_info *accounts = (struct account_info *)malloc(DB_CAPACITY * sizeof(struct account_info));

    if (db == NULL)
    {
        printf("Failed to open input file\n");
        return 0;
    }

    //For reading from the db
    while(fgets(line[dbSize], sizeof(line[1]), db))
    {
        /* note that fgets don't strip the terminating \n, checking its
        presence would allow to handle lines longer that sizeof(line) */

        //debug print statement
        printf("DB SERVER read into memory : %s \n", line[dbSize]);
        dbSize++;
    }

    for(int i = 0; i < dbSize; i++){

        //get first element
        char *accNum_ptr = strtok(line[i], ",");
        strncpy(accounts[i].acc_num, accNum_ptr, MAX_FIELD_LENGTH);

        //get next element
        char *pin_ptr = strtok(NULL, ",");
        //store the CYPHERTEXT PIN (STILL ENCRYPTED ie. this value plus 1 is the plaintext pin)
        accounts[i].pin = atoi(pin_ptr);

        //get next element
        char *balance_ptr = strtok(NULL, ",");
        accounts[i].balance = atof(balance_ptr);
    }

    fclose(db);
    return accounts;
}

//Writes the linked list of accounts to the database.
//******FREES THE ACCOUNTS POINTER*********
void pushToDb(struct account_info *accounts)
{
    /**
     * Open db file to read/write from/to
     */
    FILE *db = fopen(DB_PATH, "w+");

    if (db == NULL)
    {
        printf("Failed to open db file\n");
        return;
    }
    int i;
    for(i = 0; i < dbSize; i++){
        fprintf(db, "%s,%d,%.2f\n", accounts[i].acc_num, accounts[i].pin, accounts[i].balance);
    }

    fclose(db);
}


//Execute the transaction argument
void execute_transaction(struct transaction *command)
{
    /**
     * Complete transaction.
     */
    switch (command->action)
    {
    case UPDATE_DB:
    {
        /**
     * Open db file to read/write from/to
     */
        FILE *db = fopen(DB_PATH, "a");

        if (db == NULL)
        {
            printf("Failed to open db file\n");
            return;
        }

        fprintf(db, "%s,%d,%.2f\n", command->account.acc_num, command->account.pin, command->account.balance);
        printf("Account added to DB: %s,%d,%.2f \n", command->account.acc_num, command->account.pin, command->account.balance);
        //close database file when finished executing update
        fclose(db);
        break;
    }
    case PIN:
    {
        struct account_info *accounts = pullFromDb();
        int i;
        bool pin_ok = false;

        //look for account
        for (i = 0; i < dbSize; i++)
        {
            if (strncmp(accounts[i].acc_num, command->account.acc_num, MAX_FIELD_LENGTH) == 0)
            { //found account
                //compare against excrypted pin
                if (accounts[i].pin == command->account.pin)
                {
                    pin_ok = true;
                    currentAccount = &accounts[i];
                    badAttempts = 0;
                    replyToAtm(PIN_OK,NULL);
                }else{
                    badAttempts++;
                    //check if reached threshold attempts of 3
                    if(badAttempts > 2){
                        printf("DB SERVER : locking account %s", accounts[i].acc_num);
                        //block the account
                        accounts[i].acc_num[0] = 'x';
                        pushToDb(accounts);
                        badAttempts = 0;
                    }
                }
            }
        }
        if (!pin_ok)
        {
            replyToAtm(PIN_WRONG,NULL);
        }
        break;
    }
    case BALANCE:
    {
        if(currentAccount != NULL){
            char balance[MAX_FIELD_LENGTH];
            snprintf(balance, MAX_FIELD_LENGTH, "%.2f", currentAccount->balance);
            replyToAtm(BALANCE_REPLY, balance);
        }else{
            //Invalid action. User hasnt logged into an account.
            replyToAtm(PIN_WRONG,NULL);
        }

        break;
    }
    case WITHDRAW:
    {
        if (currentAccount != NULL)
        {
            struct account_info *accounts = pullFromDb();

            float amountToWithdraw = command->account.balance;

            int i;
            for(i = 0; i < dbSize; i++){
                if(strncmp(accounts[i].acc_num, currentAccount->acc_num, MAX_FIELD_LENGTH) == 0){

                    if (accounts[i].balance - amountToWithdraw >= 0){
                        accounts[i].balance = accounts[i].balance - amountToWithdraw;
                        currentAccount->balance = currentAccount->balance - amountToWithdraw;
                        char balanceBuf[MAX_FIELD_LENGTH];
                        snprintf(balanceBuf, MAX_FIELD_LENGTH, "%.2f", accounts[i].balance);
                        replyToAtm(FUNDS_OK, balanceBuf);
                    }else{
                        replyToAtm(NSF,NULL);
                    }
                }
            }
            pushToDb(accounts);
        }
        else
        {
            printf("Operation attempted by ATM without logging into an account.\n");
            replyToAtm(PIN_WRONG,NULL);
        }

        break;
    }
    default:
        printf("Unrecognized command attempted. Please try again.\n");
        break;
    }

}

//handler for the msg from editor to kill all running atm instances subscribed to this server
void handleAtmKill(){
    int i;
    for(i = 0; i < MAX_ATM_INSTANCES && subscribedAtms[i] != 0; i++){
        printf("DB SERVER : ATM KILL SIG received\n");
        printf("DB SERVER : killing atm instance %d...\n", subscribedAtms[i]);
        int result = kill(subscribedAtms[i], SIGTERM);
        if(result == -1){
            printf("DB SERVER : Failed to kill atm instance %d\n", subscribedAtms[i]);
        }
    }
}

//handler for the msg to lock the db file resource
void handleLockDb(){
    int wait = SemaphoreWait(s_mutex, 0);
    if (wait == IPC_ERROR)
    {
        printf("DB SERVER : Error executing a remote lock on DB\n");
    }else{
        printf("DB SERVER : DB Locked\n");
    }
}

//handler for the msg to unlock the db file resource
void handleUnlockDb()
{
    int signal = SemaphoreSignal(s_mutex);
    if (signal == IPC_ERROR)
    {
        printf("DB SERVER : Error executing remote unlock on DB\n");
        exit(1);
    }
    else
    {
        printf("DB SERVER : DB Unlocked\n");
    }
}

int main(int argc, char *argv[])
{
    //Creating semaphore, check for error
    if ((s_mutex = SemaphoreCreate(1)) == IPC_ERROR)
    {
        SemaphoreRemove(s_mutex);
        printf("DB SERVER : Error in SemaphoreCreate\n");
        exit(1);
    }

    sleep(5);
    printf("I am DATABASE_SERVER, process id: %d\n", (int)getpid());

    pid_t pid = fork();
    srand((int)pid);
    printf("fork returned: %d\n", (int)pid);
    if (pid < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    if (pid == 0)
    {
        printf("Spawned child with pid %d\n", (int)getpid());
        char * args[] = {"./dbEditor", NULL};
        execv(args[0], args);
    }

    // We must be the parent
    printf("I am the parent, waiting for child to end.\n");
    
    //DO DB SERVER MAIN LOOP

 
    int i;
    for(i = 0; i < sizeof subscribedAtms; i++){
        subscribedAtms[i] = 0;
    }
    struct my_msgbuf buf;
    int msqid;
    int toend;
    key_t key;

    //generate key for msg queue from editor
    if ((key = ftok("msgq-editor.txt", 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1)
    { /* connect to the queue */
        perror("msgget");
        exit(1);
    }


    printf("DB SERVER : message queue ready to receive messages.\n");

    badAttempts = 0;

    for (;;)
    {
        if (msgrcv(msqid, &buf, sizeof(buf.mtext), 0, 0) == -1)
        {
            perror("msgrcv");
            exit(1);
        }
        
        printf("DB SERVER recv: \"%s\"\n", buf.mtext);
        toend = strcmp(buf.mtext, "end");
        if (toend == 0)
            break;
        if(buf.mtype == 2){ //handle subscription msg
        
            //add pid to the subscribed list
            int i;
            for(i = 0; i < sizeof(subscribedAtms); i++){
                if(subscribedAtms[i] == 0){
                    int temp = 0;
                    sscanf(buf.mtext, "%d", &temp);
                    subscribedAtms[i] = temp;
                    break;
                }
            }
        }else{
            //handle the transaction
            struct transaction *transaction = (struct transaction *)malloc(sizeof(struct transaction));
            
            int param_index;

            for (param_index = 0; param_index <= MAX_ARGUMENTS; param_index++)
            {
                switch (param_index)
                {
                case 0:{
                    char *element = strtok(buf.mtext, ",");

                    if (strcmp(element, "UPDATE_DB") == 0)
                    {
                        transaction->action = UPDATE_DB;
                    }
                    else if (strcmp(element, "PIN") == 0)
                    {
                        transaction->action = PIN;
                    }

                    else if (strcmp(element, "BALANCE") == 0)
                    {
                        transaction->action = BALANCE;
                        break;
                    }
                    else if (strcmp(element, "WITHDRAW") == 0)
                    {
                        transaction->action = WITHDRAW;
                    }
                    else if (strcmp(element, "KILL") == 0){
                        handleAtmKill();
                        transaction->action = KILL;
                    }
                    else if (strcmp(element, "LOCK") == 0){
                        handleLockDb();
                        transaction->action = LOCK;
                    }
                        else
                        {
                            //UNRECOGNIZED COMMAND
                            printf("Unrecognized command attempted, please try again.\n");
                        }
                    break;}
                case 1: {
                    //See if its a withdraw command, then our second element is a withdraw value
                    if(transaction->action == WITHDRAW){
                        char *element = strtok(NULL, ",");
                        if (element == NULL)
                            break;
                        if (strstr(element, ".") != NULL)
                        { //input is a float
                            transaction->account.balance = atof(element);
                        }
                        else
                        {
                            transaction->value = atoi(element);
                        }
                    }{
                        //parsing the account number
                        char *element = strtok(NULL, ",");
                        if(element == NULL) break;
                        strncpy(transaction->account.acc_num, element, MAX_FIELD_LENGTH);
                        //null terminate string
                        transaction->account.acc_num[MAX_FIELD_LENGTH - 1] = '\0';
                    }

                    break;}
                case 2: {//parsing the  (plaintext) pin
                    char *element = strtok(NULL, ",");
                    if (element == NULL) break;
                    //store and immediately "encrypt" the pin
                    transaction->account.pin = atoi(element) - 1;
                    break;}
                case 3:{ //parsing the funds
                    char *element = strtok(NULL, ",");
                    if(element != NULL){
                        if(strstr(element, ".") != NULL)
                        { //input is a float
                            transaction->account.balance = atof(element);
                        }else{
                            transaction->value = atoi(element);
                        }
                    }
                    break;}
                default:
                    break;
                }
            }

            //check for an admin command from dbEditor shell, if not then execute the transaction
            if (transaction->action != KILL && transaction->action != LOCK && transaction->action != UNLOCK)
            {
                printf("DB SERVER : Attempting to claim db sem lock...\n");
                int wait = SemaphoreWait(s_mutex, 0);
                if(wait == IPC_ERROR){
                    printf("DB SERVER : Error waiting on semaphore\n");
                }else{
                    printf("DB SERVER : Claimed db sem lock...\n");
                    //execute the transaction
                    execute_transaction(transaction);

                    int signal = SemaphoreSignal(s_mutex);
                    if (signal == IPC_ERROR)
                    {
                        printf("DB SERVER : Error signaling on semaphore\n");
                        exit(1);
                    }else{
                        printf("DB SERVER : Released db sem lock.\n");
                    }
                }
                free(transaction);
            }
        }
    }



    printf("DB SERVER : message queue done receiving messages.\n");

    int status = 0;
    pid_t childpid = wait(&status);
    printf("DB SERVER : Parent knows child %d finished with status %d.\n", (int)childpid, status);
    int childReturnValue = WEXITSTATUS(status);
    printf("Child Return value was %d\n", childReturnValue);

    return 0;
}
