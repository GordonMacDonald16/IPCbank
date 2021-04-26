### IPCbank
# Multiprocess C application for simulating an ATM system
Author : Gordon MacDonald

## System Description
This multiprocess C ATM simulator consists of 4 processes:
	- dbServer : main server for the database. Waits for requests from other processes. Spawns a dbEditor process to offer admin operations.
	- dbEditor : a shell interface for the user to perform admin operations such as:
		- UPDATE_DB,(acc_num),(pin),(bal) (Add entry to db with account number = acc_num, PIN = pin, balance = bal)
		- KILL (Signal the dbServer to terminate any active atm instances.
		- LOCK (Signal the dbServer to manually lock the database file from access.
		- UNLOCK (Signal the dbServer to manually unlock the database file for access.
		- ctrl-d to terminate dbServer and dbEditor. (changed from project requirements to reflect industry convention)
	-atm : an instance of an ATM terminal for users to login and perform actions on their account:
		- PIN,(acc_num),(pin) (attempt to login to account=acc_num and PIN=pin)
		Once receiving PIN_OK from dbServer...
		- BALANCE (return balance of the account)
		- WITHDRAW,(amnt) (withdraws amount-amnt from the account)
	-interest : an interest calculator that will execute every 60 seconds (from running) to accrue interest on the accounts in the database.
	
	
## Build and Running
- navigate to the main application directory that contains the files: dbServer.c, dbEditor.c, atm.c, interest.c, Makefile, Readme.md(this), msgq-atm.txt, msgq-editor.txt, db-semaphore.txt and db.txt
- run the following commands in cmd (within project directory):

>>make

>>.\dbServer

You will be promted with the dbEditor (dbServer spawns the dbEditor via fork) shell to enter UPDATE_DB commands which allow an admin user to add account entries to the database (db.txt) this should be the only way that the db file is edited to ensure that each entry has a trailing newline character. It is essential that you run the dbServer prior to running either the interest process (iterates the db to apply 'interest' acrual), or any atm processes.


To launch an atm.c:
-open a second instance of cmd, run:

>>./atm

To initiate interest.c:
-open a second instance of cmd, run:

>>./interest
This will initiate the countdown timer, when 1 minute elapses it will interate the db and accrue interest on the accounts.


## Testing:
	A test case file has been provided: testCase1.txt that contains steps for testing the system. I have included a sample output from both processes to show what the proper output should look like after executing the test (testOutput.png)

## Current Features:
	-Full dbEditor and dbServer relationship. dbEditor can be used to add accounts to db and should be the only way that accounts are modified.
	-atm can perform 'login' (PIN message) that is validated by dbServer.
	-dbServer will block an account after 3 consecutive failed access attempts.
	-atm can check the balance of the account if they have a successful login
	-atm can withdraw available funds and is served with an NSF if the funds are insufficient.
	-interest will iterate every 60 seconds through the db (if the resource semaphore lock is available) and apply an increase of 1% to positive account balances and a decrease of 2% to negative account balances.
	
	BONUS Features: Admin Security Operations (for dbEditor in the event of security breaches)
	- KILL (Signal the dbServer to terminate any active atm instances.
	- LOCK (Signal the dbServer to manually lock the database file from access.
	- UNLOCK (Signal the dbServer to manually unlock the database file for access. 
	

## Missing Features:
	-Output file for process states and a semaphore lock for it (To generate deadlock. Currently state outputs are just printf's)
	-Deadlock scenario state output

	

## Troubleshooting:
	-We use token files to link the msg queues and semaphores between processes, these files MUST exist for the system to run(msgq-editor.txt, msgq-atm.txt, db-semaphore.txt)
	-If the queues are out of sync they will complain about missing identifiers. Just kill the running processes and re-run dbServer and atm.
	-To exit any running processes that are waiting for input press ctrl-d
	-If stuck in loop the global escape is ctrl-c
	-If the interest process is stuck trying to claim the db sem lock. That means dbServer needs to be run first. Kill interest with ctrl-c and run dbServer prior to restarting interest.


