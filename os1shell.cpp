/**
*
* File: 		os1shell.cpp
*
* Author: 		Grant Kurtz
* Contributors:	(See the file 'Resources' for a full list of references used)
*
* Description:	Parses user input and runs the associated Linux utility.  Also
*				Keeps a running history of previously entered user commands.
*				^D will quit the program, and ^C will print the last 20
*				commands entered by the user.
*
*/

#include <iostream>
#include <stdio.h>
#include <string>
#include <signal.h>

using namespace std;

// CONSTANTS
int MAX_BUF_SIZE = 64;
int MAX_HIST_LEN = 20;
unsigned int DEFAULT_CSIZE = 8;
unsigned int DEFAULT_SIZE = 10;

// a node struct for our doubly-linked list
typedef struct node{
	char command[64];
	node *next;
	node *prev;
};

typedef struct mbr{
	unsigned int cluster_size;
	unsigned int disk_size;
	unsigned int dir_table_index;
	unsigned int FAT_index;
};

typedef struct directory{
	char name[112];
	unsigned int index;
	unsigned int size;
	unsigned int type;
	unsigned int timestamp;
};

// globals
node *history = NULL;
node *tail = NULL;
char curDirectory[128];
bool donotread = false;
unsigned int MAX_FILES;

// functions
void printHistory(node *history);
void handler_function(int sig_id);
char* trim(char* str);
void resetBuf(char* buf);
void processTerminated(int childPID);
void clearInput();
bool checkFSIntegrity(mbr * MBR);

/*
* The mother of all main functions
*/
int main(int argc, char *argv[]){

	// vars
	char buf[MAX_BUF_SIZE-1];
	char* tokens;
	bool alive;
	unsigned int curHistSize = 0;
	char fsname[sizeof(argv[1])+1];
	FILE * filesystem;
	memcpy(&fsname, argv[1], sizeof(argv[1])+2);
	
	// make sure we got a clean slate after creating the buffer
	resetBuf(buf);
	
	// check if the filesystem already exists
	filesystem = fopen(fsname, "r");
	if(!filesystem){
		
		// don't leave the filepointer open
		fclose(filesystem);
		
		// vars
		int fs_size, fs_csize;
		
		// the FS does not exist, let's make sure the user actually wants to
		// create one
		printf("Are you sure you want to create a new file system [Y]? ");
		fflush(stdout);
		int r = read(0, buf, MAX_BUF_SIZE);
		
		if(r == 1 || strcmp(buf, "y")){
		
			printf("Enter the maximum size for this file system in MB "
					"[10]: ");
			fflush(stdout);
			resetBuf(buf);
			r = read(0, buf, MAX_BUF_SIZE);
			
			// the below ternary operator is horrible, will remove in future
			// update
			fs_size = r == 1 ? DEFAULT_SIZE : atoi(buf);
			
			// keep re-asking until a valid value is given
			while(r != 1 && (fs_size > 50 || fs_size < 5)){
				
				// the user can't do that...
				cerr << "That is not a valid filesize.  Valid integer"
					 " values are 5..50\n";
				
				// re-ask the user for the filesystem size
				printf("Enter the maximum size for this file system in MB "
						"[10]: ");
				fflush(stdout);
				resetBuf(buf);
				r = read(0, buf, MAX_BUF_SIZE);
				fs_size = r == 1 ? DEFAULT_SIZE : atoi(buf);
			}
			
			// prompt user for the cluster size
			printf("Enter the cluster size for this file system in KB [8]: ");
			fflush(stdout);
			resetBuf(buf);
			r = read(0, buf, MAX_BUF_SIZE);
			fs_csize = r == 1 ? DEFAULT_CSIZE : atoi(buf);
			
			// keep re-asking until a valid value is given
			while(r != 1 && (fs_csize > 16 || fs_csize < 8)){
				
				// the user can't do that...
				cerr << "That is not a valid cluster size.  Valid integer"
					 " values are 8..16\n";
				
				// re-ask the user for the filesystem size
				printf("Enter the cluster size for this file system in KB "
						"[8]: ");
				fflush(stdout);
				resetBuf(buf);
				r = read(0, buf, MAX_BUF_SIZE);
				fs_csize = r == 1 ? DEFAULT_CSIZE : atoi(buf);
			}
			
			// now that we have a max filesystem size and a cluster size, we
			// can compute the maximum number of files that can be recorded
			MAX_FILES = (fs_size*1024*1024)/(fs_csize*1024);
			
			// lets also determine if we can actually store all those records
			// in our File Allocation Table
			if(fs_csize*1024 < MAX_FILES*4){
				cerr << "Whoops! Looks like you need to make the cluster"
						" size a little larger or reduce the maximum size of"
						" your filesystem!  The FAT can't fit!\n";
				exit(1); // for now just exit--later repromt user for data
			}
			
			// create the struct to store our filesystem data
			mbr *MBR = (mbr*)malloc(sizeof(mbr));
			MBR->cluster_size = fs_csize;
			MBR->disk_size = fs_size;
			MBR->dir_table_index = 1;
			MBR->FAT_index = 2;
			
			// actually create the filesystem on the disk by "jumping" to the
			// location of the file that will be the size of our filesystem
			// close off the file with a NUL-byte
			filesystem = fopen(fsname, "w");
			fseek(filesystem, fs_size*1024*1024, SEEK_SET);
			char zero[] = {'\0'};
			fwrite(&zero, 1, 1, filesystem);
			
			// write the MBR to the filesystem
			fseek(filesystem, 0, SEEK_SET);
			fwrite(MBR, sizeof(int), sizeof(MBR), filesystem);
		}
	}
	else{
		
		// since the filesystem already exists, load its MBR into memory
		mbr *MBR = (mbr*)malloc(sizeof(mbr));
		fread(MBR, sizeof(int), sizeof(MBR), filesystem);
		
		// do some basic checking to make sure the MBR isn't corrupt or
		// worthless		
		if(checkFSIntegrity(MBR)){
			
			// prompt user asking if they really want to keep using the
			// specified filesystem
			printf("Are you sure you still want to use this filesystem [N]? ");
			fflush(stdout);
			int r = read(0, buf, MAX_BUF_SIZE);
			if(strcmp(buf, "n") || r == 1){
				
				// delete the MBR
				free(MBR);
				
				// clear the filesystem name
				memset(fsname, sizeof(fsname), '\0');
				
				// let the user know that the filesystem has been discarded
				cout << "Filesystem not loaded!\n";
			}
			
			// Alright, then the user must know what they are doing!
			else if(strcmp(buf, "y")){
				cout << "Alright, the filesystem will try to be loaded...\n";
			}
		}
	}

	// define our signal handler
	struct sigaction signal_action;
	signal_action.sa_handler = handler_function;
	signal_action.sa_flags = 0;
	sigemptyset(&signal_action.sa_mask);
	
	// tell the signal handler that we want to listen to the list of signals
	// below
	sigaction(SIGHUP, &signal_action, NULL);
	sigaction(SIGINT, &signal_action, NULL);
	sigaction(SIGQUIT, &signal_action, NULL);
	sigaction(SIGILL, &signal_action, NULL);
	sigaction(SIGTRAP, &signal_action, NULL);
	sigaction(SIGABRT, &signal_action, NULL);
	sigaction(SIGEMT, &signal_action, NULL);
	sigaction(SIGFPE, &signal_action, NULL);
	sigaction(SIGKILL, &signal_action, NULL);
	sigaction(SIGBUS, &signal_action, NULL);
	sigaction(SIGSEGV, &signal_action, NULL);
	sigaction(SIGSYS, &signal_action, NULL);
	sigaction(SIGPIPE, &signal_action, NULL);
	sigaction(SIGALRM, &signal_action, NULL);
	sigaction(SIGTERM, &signal_action, NULL);
	sigaction(SIGUSR1, &signal_action, NULL);
	sigaction(SIGUSR2, &signal_action, NULL);
	sigaction(SIGCHLD, &signal_action, NULL);
	sigaction(SIGPWR, &signal_action, NULL);
	sigaction(SIGWINCH, &signal_action, NULL);
	sigaction(SIGURG, &signal_action, NULL);
	sigaction(SIGPOLL, &signal_action, NULL);
	sigaction(SIGSTOP, &signal_action, NULL);
	sigaction(SIGTSTP, &signal_action, NULL);
	sigaction(SIGCONT, &signal_action, NULL);
	sigaction(SIGTTIN, &signal_action, NULL);
	sigaction(SIGTTOU, &signal_action, NULL);
	sigaction(SIGVTALRM, &signal_action, NULL);
	sigaction(SIGPROF, &signal_action, NULL);
	sigaction(SIGXCPU, &signal_action, NULL);
	sigaction(SIGXFSZ, &signal_action, NULL);
	sigaction(SIGWAITING, &signal_action, NULL);
	
	// initialization setup
	alive = true;
	resetBuf(buf);
	
	// continue reading input forever
	while(alive){
		
		// provide prompt and wait for input
		write(0, "OS1Shell -> ", 12);
		int r = read(0, buf, MAX_BUF_SIZE);
		
		// handle odd read values
		// if nothing was read, assume ^D was sent
		if(r == 0){
			cout << endl;
			return 0;
		}
		
		// if we read a single character, assume nothing was sent
		else if(r == 1){
			fflush(stdout);
			continue;
		}
		
		// if we read a full buffer but the last character is not a newline
		// then the user attempted to input a command >64 characters
		if(r == 64 && buf[63] != '\n'){
			cerr << "Your command is too long!\n";
			
			clearInput();
		
			// clear the buffer
			resetBuf(buf);
			continue;
		}
		
		// if we got here, then the command is syntactically correct, and needs
		// to be semantically broken down
		
		// trim all whitespace
		trim(buf);
		if(buf[0] == 0){
			
			// don't print an error if it is a signal
			if(!donotread){
				cerr << "All this space is killing me!\n";	
			}
			else{
				donotread = false;
			}
			resetBuf(buf);
			continue;
		}
		
		// alright, the command is good, add to our history for recall
		if(curHistSize == 0){
		
			// create our first node
			history = (node*)malloc(sizeof(node));
			tail = history;
			history->prev = 0;
			history->next = 0;
			
			// copy over the command's contents (excluding the new-line)
			strncpy(history->command, buf, r-1);			
			curHistSize = 1;
		}
		else{
		
			// create the new node
			node *com;
			com = (node*)malloc(sizeof(node));
			
			// properly link everything for the linked list
			tail->next = com;
			com->prev = tail;
			tail = com;
			tail->next = 0;
			
			// copy over the command's contents (excluding the new-line) 
			strncpy(com->command, buf, r-1);
			curHistSize += 1;
			
			// make sure our history doesn't grow beyond the max size
			if(curHistSize > MAX_HIST_LEN){
			
				// temp copy of head node
				node *toBeFreed = history;
				
				// set the head to be the 2nd oldest command
				history = history->next;
				
				// we don't need the old head anymore, free it up
				free(toBeFreed);
				curHistSize -= 1;
			}
		}
		
		// tokenize the string
		char *args[r];
		tokens = strtok(buf, " \n");
		int i = 0;
		bool runInBG = false;
		
		// read all tokens
		while((args[i] = tokens) != NULL){
			tokens = strtok(NULL, " \n");
			
			// if a token contains the &, then we need to run the command
			// in the background
			if(tokens != NULL && strcmp(tokens, "&") == 0){
				runInBG = true;
			}
			i++;
		}
		
		// if we read an argument, pad the end of the array with a nul-byte
		if(i > 1)
			args[i-1] = (char*)0;
		
		// check if we are running a shell-specific command
		if(strcmp(buf, "history") == 0){
			printHistory(history);
			continue;
		}
		
		// Run the command
		pid_t childPID;
		int childStatus;
		
		// create a new process
		childPID = fork();
		
		// If zero, then this is the child running
		if(childPID == 0){
			
			// execute the command			
			execvp(args[0], args);
			
			// if we reached here, the command was invalid
			cerr << "\nBad command!" << endl;
		}
		else if(!runInBG){
			
			// this is done by the parent
			pid_t tpid;
			do{
				tpid = wait(&childStatus);
				
				// catch any processes that may have terminated while we were
				// busy with the user
				if(tpid != childPID) processTerminated(tpid);
			}while(tpid != childPID);
			
			//cout << "Status of Child: " << childStatus << endl;
		}
		
		// make sure nothing is sitting in the buffer
		fflush(stdout);
		
		// clear the input buffer for the next read
		resetBuf(buf);
	}
}

/*
* Prints the last MAX_HIST_LEN of valid commands the user has entered.
*
* @param	comHist			pointer to the head (last command entered) of the
*							history list
*/
void printHistory(node *comHist){
	while(comHist != NULL){
		cout << comHist->command << endl;
		comHist = comHist->next;
	}
}

/*
* Handles processing of all signals captured by our process
*
* @param	sig_id			an integer representing the signal that was 
*							captured
*/
void handler_function(int sig_id){
	if(sig_id == 2){
		cout << endl;
		printHistory(history);
	}
	else
		cout << "\nCaught Signal: " << sig_id << endl;
	donotread = true;
}

/*
* Trims all leading and trailing whitespace
* Source: (Reference: 4)
*
* @param	str			the string to rid of whitespace
*
* @returns				a pointer to the modified char array
*/
char *trim(char *str){
	size_t len = 0;
	char *frontp = str - 1;
	char *endp = NULL;

	if( str == NULL )
		return NULL;

	if( str[0] == '\0' )
		return str;

	len = strlen(str);
	endp = str + len;

	/* Move the front and back pointers to address
	* the first non-whitespace characters from
	* each end.
	*/
	while( isspace(*(++frontp)) );
	while( isspace(*(--endp)) && endp != frontp );

	if( str + len - 1 != endp )
		*(endp + 1) = '\0';
	else if( frontp != str &&  endp == frontp )
		*str = '\0';

	endp = str;
	if( frontp != str ){
		while( *frontp ) *endp++ = *frontp++;
			*endp = '\0';
	}

	return str;
}

/*
* Just prints that a process we had forked and ran in the background finished 
* running and has exited.
*
* @param	childPID		the process ID of the process that just exit
*/
void processTerminated(int childPID){
	
	// don't print termination messages if the process didn't come from a 
	// forked background process
	if(childPID > 0)
		cout << "Child Process terminated: " << childPID << endl;
}

bool checkFSIntegrity(mbr * MBR){
	
	bool problemsFound = false;
	
	if(MBR->cluster_size < 8){
		cerr << "Looks like this filesystem's cluster size is really " 
				"small!\n This could cause problems with reading/writing "
				"files.\n";
		problemsFound = true;
	}
	else if(MBR->cluster_size > 16){
		cerr << "This filesystem uses an abnormally large cluster size;\n"
				"this shouldn't cause problems, however.\n";
		problemsFound = true;
	}
	if(MBR->disk_size < 5){
		cerr << "Warning! This filesystem is unusually small! This is not "
				"necessarily a problem, but should be made bigger.\n";
		problemsFound = true;
	}
	else if(MBR->disk_size > 50){
		cerr << "This filesystem is abnormally large in size;\n"
				"this shouldn't cause problems, however.\n";
		problemsFound = true;
	}
	
	// make sure the locations of the directory table/FAT are at least
	// non-suspect
	if(MBR->FAT_index < 1){
		cerr << "The filesystem's FAT appears to be in a non-standard"
				" location!\n";
		problemsFound = true;
	}
	if(MBR->dir_table_index < 1){
		cerr << "The filesystem's directory table appears to be in a"
				"non-standard location!\n";
		problemsFound = true;
	}
	else if(MBR->dir_table_index == MBR->FAT_index){
		cerr << "The filesystem's FAT and directory table appear to be"
				" in the same location!\n";
		problemsFound = true;
	}
	
	return problemsFound;
}

/*
* Resets all characters of a given char array to nul-bytes
*
* @param	buf				character array to be cleaned up
*/
void resetBuf(char* buf){
	memset(buf, 0, sizeof(buf));
}

void clearInput(){
	int ch = 0;
	while((ch = getc(stdin)) != EOF && ch != '\n' && ch != '\0');
	fflush(stdout);
}

