#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

#define MAX_FIELD_LENGTH 10
#define PERMS 0644
#define SERVER_QUEUE "msgq-editor.txt"
#define LISTEN_QUEUE "msgq-atm.txt"

struct my_msgbuf
{
    long mtype;
    char mtext[200];
};

//Waits for a reply from server and returns a string of it's reply
char *waitForServerResponse()
{

    struct my_msgbuf buf;
    int msqid;
    key_t key;

    //generate key for msg queue from editor
    if ((key = ftok(LISTEN_QUEUE, 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }
    msqid = msgget(key, PERMS | IPC_CREAT);

    if(msqid == -1)
    { /* connect to the queue */
        perror("msgget");
        exit(1);
    }

    printf("ATM %d: waiting for server response...\n", (int)getpid());

    for (;;)
    {
        if (msgrcv(msqid, &buf, sizeof(buf.mtext), 0, 0) == -1)
        {
            perror("msgrcv");
            exit(1);
        }

        printf("ATM received: \"%s\"\n from DB SERVER \n", buf.mtext);
        char *bufptr = buf.mtext;
        return bufptr;
    }
}

void waitForUserTransaction(){
    struct my_msgbuf buf;
    int msqid;
    int len;
    key_t key;

    if ((key = ftok(SERVER_QUEUE, 'B')) == -1)
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
    

    printf("Perform operations on your account by sending commands such as: \n\
            \t\t BALANCE\n\
            \t\t WITHDRAW,amount \n\
            (^D to quit):\n");

        while (fgets(buf.mtext, sizeof buf.mtext, stdin) != NULL)
        {
            len = strlen(buf.mtext);
            /* remove newline at end, if it exists */
            if (buf.mtext[len - 1] == '\n')
                buf.mtext[len - 1] = '\0';
            if (msgsnd(msqid, &buf, len + 1, 0) == -1) /* +1 for '\0' */
                perror("msgsnd");
            char *reply = waitForServerResponse();
            char *type = strtok(reply, ",");

            if (strncmp(type, "FUNDS_OK", MAX_FIELD_LENGTH) == 0)
        {
            char *balance_ptr = strtok(NULL, ",");
            printf("ATM : Funds Withdrawn. New balance is $%.2f\n", atof(balance_ptr));
        }
        else if (strncmp(type, "BALANCE", MAX_FIELD_LENGTH) == 0)
        {
            char *balance_ptr = strtok(NULL, ",");
            printf("ATM : Balance returned is $%.2f\n", atof(balance_ptr));
        }
    }
    strcpy(buf.mtext, "end");
    len = strlen(buf.mtext);
    if (msgsnd(msqid, &buf, len + 1, 0) == -1) /* +1 for '\0' */
        perror("msgsnd");

    if (msgctl(msqid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl");
        exit(1);
    }
}




int main(int argc, char const *argv[])
{
    struct my_msgbuf buf;
    int msqid;
    int len;
    key_t key;

    bool isSubscribed = false;

    if ((key = ftok(SERVER_QUEUE, 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1)
    {
        perror("msgget");
        exit(1);
    }

    printf("ATM instance id: %d ready to login.\n", (int)getpid());
    printf("Enter your account info (account number and PIN) the command format: \n \
            \t\t PIN,ACC_NUM,PIN \n\
            After receiving a PIN_OK message from the server perform operations such as: \n\
            \t\t BALANCE,\n\
            \t\t WITHDRAW,amount \n\
            (^D to quit):\n");
    //check if already subscribed to the dbServer, if not send a msgtype 0 to it.
    if (!isSubscribed)
    {
        buf.mtype = 2;
        pid_t pid = getpid();
        sprintf(buf.mtext, "%d", pid);
        len = strlen(buf.mtext);
        if (msgsnd(msqid, &buf, len + 1, 0) == -1) /* +1 for '\0' */
            perror("msgsnd");
        isSubscribed = true;
    }

        buf.mtype = 1; /* we don't really care in this case */
    

        while (fgets(buf.mtext, sizeof buf.mtext, stdin) != NULL)
        {
            len = strlen(buf.mtext);
            /* remove newline at end, if it exists */
            if (buf.mtext[len - 1] == '\n')
                buf.mtext[len - 1] = '\0';
            if (msgsnd(msqid, &buf, len + 1, 0) == -1) /* +1 for '\0' */
                perror("msgsnd");

            //get reply from server
            char *reply = waitForServerResponse();

            if (strncmp(reply, "PIN_OK", MAX_FIELD_LENGTH) == 0)
            {
                waitForUserTransaction();
            }
            else if (strncmp(reply, "PIN_WRONG", MAX_FIELD_LENGTH) == 0)
            {
                printf("ATM : Unsuccessful login attempt. \n \
            Please try again, Warning: 3 consecutive failed attempts will lock the account\n");
                char *args[] = {"./atm", NULL};
                //run itself from main loop again...
                execv(args[0], args);
            }
        }
        strcpy(buf.mtext, "end");
        len = strlen(buf.mtext);
        if (msgsnd(msqid, &buf, len + 1, 0) == -1) /* +1 for '\0' */
            perror("msgsnd");

        if (msgctl(msqid, IPC_RMID, NULL) == -1)
        {
            perror("msgctl");
            exit(1);
        }
    }
