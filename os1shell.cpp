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
bool donotread = false;

// a node struct for our doubly-linked list
typedef struct node{
	char command[64];
	node *next;
	node *prev;
};

// globals
node *history = NULL;
node *tail = NULL;

// functions
void printHistory(node *history);
void handler_function(int sig_id);
char *trim(char *str);
void resetBuf(char* buf);
void processTerminated(int childPID);


int main(int argc, char *argv[]){
	
	// first, let's check that we got the correct number of arguments
	if(argc != 2){
		cerr << "Usage: " << *argv << " filesystem\n";
		exit(1);
	}

	// vars
	char buf[MAX_BUF_SIZE-1];
	char* tokens;
	bool alive;
	int curHistSize = 0;
	char fsname[sizeof(argv[1])];
	FILE * filesystem;
	memcpy(&fsname, argv[1], sizeof(argv[1]));
	
	
	// check if the filesystem already exists
	filesystem = fopen(fsname, "r");
	if(!filesystem){
		
		// looks like we need to create the filesystem
		//filesystem = createFileSystem(&fsname);
		
		// set our newly created filesystem as our current directory
		
	}
	else{
		
		// since the filesystem already exists, load its BR into memory and
		// make sure everything checks out
		
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
			
			// clear the input stream
			int ch = 0;
			while((ch = getc(stdin)) != EOF && ch != '\n' && ch != '\0');
			fflush(stdout);
		
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

/*
* Resets all characters of a given char array to nul-bytes
*
* @param	buf				character array to be cleaned up
*/
void resetBuf(char* buf){
	memset(buf, 0, sizeof(buf));
}


