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

#define PERMS 0644 
#define DB_PATH "db.txt"
#define MAX_ARGUMENTS 3
#define DB_CAPACITY 25
#define MAX_FIELD_LENGTH 10



enum transaction_type{
    UPDATE_DB = 0,
    PIN = 1,
    BALANCE = 2,
    WITHDRAW = 3
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
};

struct transaction
{
    enum transaction_type action;
    struct account_info account;
    int value;
};

struct db_info{
    int dbSize;
    struct account_info *accounts;
};

void execute_transaction(struct transaction *command)
{
    /**
     * Open db file to read/write from/to
     */
    FILE *db  = fopen(DB_PATH, "r+");

    if (db == NULL)
    {
        printf("Failed to open db file\n");
        return;
    }

    /**
     * Complete transaction.
     */
    switch (command->action)
    {
    case UPDATE_DB:
        fprintf(db, "%s,%d,%.2f \n", command->account.acc_num, command->account.pin, command->account.balance);
        printf("Account added to DB: %s,%d,%.2f \n", command->account.acc_num, command->account.pin, command->account.balance);
        break;
    default:
        printf("Unrecognized command attempted. Please try again.");
        break;
    }
    //close database file when finished executing trans
    fclose(db);
}

//Refresh the accounts array by allocating space for the current acocunts
//return pointer to the first account
//**************CALLER MUST FREE THE POINTER THAT IS RETURNED**********
struct db_info *pullFromDb(){
    int dbSize = 0;

    char line[DB_CAPACITY][MAX_FIELD_LENGTH * MAX_ARGUMENTS];
    FILE *db = fopen(DB_PATH, "r");
    struct account_info *accounts = (struct account_info *)malloc(DB_CAPACITY * sizeof(struct account_info));
    struct db_info db_info;

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
        printf("DB SERVER read into memory : %s", line[dbSize]);
        dbSize++;
    }
    db_info->dbSize = dbSize;

    for(int i = 0; i < dbSize; i++){

        //get first element
        char *accNum_ptr = strtok(line[i], ",");
        strncpy(accounts[i].acc_num, accNum_ptr, MAX_FIELD_LENGTH);

        //get next element
        char *pin_ptr = strtok(NULL, ",");
        //store the CYPHERTEXT PIN (STILL ENCRYPTED ie. this value subtract 1 is the plaintext pin)
        accounts[i].pin = atoi(pin_ptr);

        //get next element
        char *balance_ptr = strtok(NULL, ",");
        accounts[i].balance = atof(balance_ptr);

    }

    fclose(db);
    db_info->accounts = accounts;
    return db_info;
}

//Writes the linked list of accounts to the database

void pushToDb(struct account_info *accounts){
    /**
     * Open db file to read/write from/to
     */
    FILE *db = fopen(DB_PATH, "r");

    if (db == NULL)
    {
        printf("Failed to open db file\n");
        return;
    }

    while(accounts != NULL){
        fprintf(db, "%s,%d,%.2f \n", accounts->acc_num, accounts->pin, accounts->balance);
        accounts++;
    }

    fclose(db);

}


int main(int argc, char *argv[])
{

    system("touch msgq.txt");

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
    struct my_msgbuf buf;
    int msqid;
    int toend;
    key_t key;

    //generate key for msg queue
    if ((key = ftok("msgq.txt", 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1)
    { /* connect to the queue */
        perror("msgget");
        exit(1);
    }




    pushToDb(accounts);

    printf("DB SERVER : message queue ready to receive messages.\n");

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
                }
                else if (strcmp(element, "WITHDRAW") == 0)
                {
                    transaction->action = WITHDRAW;
                }
                else
                {
                    //UNRECOGNIZED COMMAND
                    printf("Unrecognized command attempted, please try again.");
                }
                break;}
            case 1: {//parsing the account number
                char *element = strtok(NULL, ",");
                strncpy(transaction->account.acc_num, element, MAX_FIELD_LENGTH);
                transaction->account.acc_num[MAX_FIELD_LENGTH - 1] = '\0';

                break;}
            case 2: {//parsing the  (plaintext) pin
                char *element = strtok(NULL, ",");

                //store and immediately "encrypt" the pin
                transaction->account.pin = atoi(element) + 1;

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

        //execute the transaction
        execute_transaction(transaction);
        free(transaction);



    }



    printf("DB SERVER : message queue done receiving messages.\n");
    system("rm msgq.txt");

    int status = 0;
    pid_t childpid = wait(&status);
    printf("DB SERVER : Parent knows child %d finished with status %d.\n", (int)childpid, status);
    int childReturnValue = WEXITSTATUS(status);
    printf("Child Return value was %d\n", childReturnValue);

    return 0;
}
