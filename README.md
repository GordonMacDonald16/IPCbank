### IPCbank
# Multiprocess C application for simulating an ATM system

## Build and Running
- navigate to the main application directory that contains the files: dbServer.c, dbEditor.c, Makefile, Readme.md(this), msgq.txt and db.txt
- run the following commands in terminal:
>>make
>>.\dbServer

You will be promted with the dbEditor shell to enter UPDATE_DB commands which allow an admin user to add account entries to the database (db.txt).


## In progress:
  Task:
-New process (file atm.c)

Features:
	-Must connect to the same msgQueue as the dbServer and dbEditor
	-Must be able to send AND receive messages from the dbServer (see dbServer for receiving and see dbEditor for sending. Will need a new msg queue for the atm<->dbServer communication, and a new queue token file)


