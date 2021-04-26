#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>

#define PERMS 0644

struct my_msgbuf
{
    long mtype;
    char mtext[200];
};


int main(void)
{
    struct my_msgbuf buf;
    int msqid;
    int len;
    key_t key;

    if ((key = ftok("msgq-editor.txt", 'B')) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1)
    {
        perror("msgget");
        exit(1);
    }

    printf("DB EDITOR : message queue ready to send messages to DB SERVER.\n");
    printf("Enter an update using the command format: \n \
            \t\t UPDATE_DB,ACC_NUM,PIN,BALANCE (^D to quit):\n");
    buf.mtype = 1; /* we don't really care in this case */

    while (fgets(buf.mtext, sizeof buf.mtext, stdin) != NULL)
    {
        len = strlen(buf.mtext);
        /* remove newline at end, if it exists */
        if (buf.mtext[len - 1] == '\n')
            buf.mtext[len - 1] = '\0';
        if (msgsnd(msqid, &buf, len + 1, 0) == -1) /* +1 for '\0' */
            perror("msgsnd");
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
    
    printf("DB EDITOR message queue: done sending messages.\n");

    

    return 0;
}