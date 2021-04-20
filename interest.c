#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/sem.h>
#include <string.h>

#define DB_PATH "db.txt"
#define PERMS 0644
#define MAX_ARGUMENTS 3
#define DB_CAPACITY 25
#define MAX_FIELD_LENGTH 10
#define IPC_ERROR -1
#define DB_SEM_FNAME "db-semaphore.txt"


//tracks dbSize which can change at any time due to ipc
volatile int dbSize;
int s_mutex;


//structure for accounts
struct account_info
{
    char acc_num[MAX_FIELD_LENGTH];
    int pin;
    float balance;
    int failedAccessAttempts;
};

//attempts to decrement sem value. 
//If impossible -> sem value = 0, blocks this process until sem value > 0 (dbServer releases lock)
int SemaphoreWait(int semid, int iMayBlock)
{
    struct sembuf sbOperation;
    sbOperation.sem_num = 0;
    sbOperation.sem_op = -1;
    sbOperation.sem_flg = iMayBlock;
    return semop(semid, &sbOperation, 1);
}

//increments sem value 
int SemaphoreSignal(int semid)
{
    struct sembuf sbOperation;
    sbOperation.sem_num = 0;
    sbOperation.sem_op = +1;
    sbOperation.sem_flg = 0;
    return semop(semid, &sbOperation, 1);
}

//Removes Sem instance
void SemaphoreRemove(int semid)
{
    if (semid != IPC_ERROR)
        semctl(semid, 0, IPC_RMID, 0);
}

//Creates a sem object from the linked token with dbServer 
//(which must already have run to initialize)
int SemaphoreCreate()
{
    int semid;

    key_t key;

    if ((key = ftok(DB_SEM_FNAME, 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }
    
    /* get the semaphore */
    semid = semget(key, 1, PERMS | IPC_CREAT);
    /* check for errors */
    if (semid == IPC_ERROR)
        return semid;

    return semid;
}

//Refresh the accounts array by allocating space for the current acocunts
//return pointer to the first account
//**************PUSH MUST FREE THE POINTER THAT IS RETURNED**********
struct account_info *pullFromDb()
{
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
    while (fgets(line[dbSize], sizeof(line[1]), db))
    {
        /* note that fgets don't strip the terminating \n, checking its
        presence would allow to handle lines longer that sizeof(line) */

        //debug print statement
        printf("DB SERVER read into memory : %s \n", line[dbSize]);
        dbSize++;
    }

    for (int i = 0; i < dbSize; i++)
    {

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
//******FREE THE ACCOUNTS POINTER*********
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
    for (i = 0; i < dbSize; i++)
    {
        fprintf(db, "%s,%d,%.2f\n", accounts[i].acc_num, accounts[i].pin, accounts[i].balance);
    }

    fclose(db);
}

int main(int argc, char const *argv[])
{

    s_mutex = SemaphoreCreate();

    //check error. dbServer must be running to initialize the semaphore lock
    if(s_mutex == IPC_ERROR){
        printf("INTEREST : Failed to link with sem lock. \n\
        Please ensure that dbServer is running without errors.\n");
        exit(1);
        }
    else{

        

        printf("INTEREST : Countdown started at 60 sec...\n");
        sleep(60);

        printf("INTEREST : Attempting to claim db sem lock...\n");
        int waitResult = SemaphoreWait(s_mutex, 0);
        if(waitResult == -1)
        {
            printf("INTEREST : Error waiting on semaphore\n");
        }else{
            printf("INTEREST : Lock obtained. Applying interest accrual to db...\n");
            struct account_info *accounts = pullFromDb();
            int i;
            for(i = 0; i < dbSize; i++){
                float prev = accounts[i].balance;
                if(prev >= 0){
                    accounts[i].balance += (0.01 * prev);
                }else{
                    accounts[i].balance -= (0.02 * prev);
                }
            }
            pushToDb(accounts);
            int sigResult = SemaphoreSignal(s_mutex);
            if(sigResult == IPC_ERROR){
                printf("INTEREST : Error signaling on semaphore\n");
            }else{
                printf("INTEREST : Released db sem lock.\n");
                printf("INTEREST : Restarting process...\n\n\n\n");

                //run this process again
                char *args[] = {"./interest", NULL};
                execv(args[0], args);
            }

        }
    }
    return 1;
}
