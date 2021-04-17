### IPCbank
# Multiprocess C application for simulating an ATM system

## Build and Running
- navigate to the main application directory that contains the files: dbServer.c, dbEditor.c, Makefile, Readme.md(this), msgq-atm.txt, msgq-editor.txt and db.txt
- run the following commands in cmd (within project directory):
>>make
>>.\dbServer

You will be promted with the dbEditor (dbServer spawns the dbEditor) shell to enter UPDATE_DB commands which allow an admin user to add account entries to the database (db.txt).

To use Atm.c:
-open a second instance of cmd, run make
-run atm : ./atm

## Testing:
	A test case file has been provided: testCase1.txt that contains steps for testing the system. I have included a sample output from both processes to show what the proper output should look like after executing the test (testOutput.png)

## Current Features:
	-Full dbEditor and dbServer relationship.
	-Atm can perform 'login' (PIN message) that is validated by dbServer
	-dbServer will block an account after 3 consecutive failed access attempts
  
## Missing Features:
	-Currently dealing with a bug that is preventing the balance and withdraw message replies from being sent back from the dbServer. (This is apparent when attempting a BALANCE command after a successful PIN command)

## Troubleshooting:
	-We use token files to link the msg queues between processes, these files must exist for the system to run(msgq-editor.txt, msgq-atm.txt)
	-If the queues are out of sync they will complain about missing identifiers. Just kill the running processes and re-run dbServer and atm.
	-To exit any running processes that are waiting for input press ctrl-d
	-If stuck in loop the global escape is ctrl-c


