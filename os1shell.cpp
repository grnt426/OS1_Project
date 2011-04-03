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

typedef struct node{
	char command[64];
	node *next;
	node *prev;
};

// globals
node *history = NULL;
node *tail = NULL;

void printHistory(node *history);
void handler_function(int sig_id);
char *trim(char *str);
void resetBuf(char* buf);
void processTerminated(int childPID);


int main(int argc, char *argv[]){
		
	// vars
	char buf[MAX_BUF_SIZE-1];
	char* tokens;
	bool alive;
	int curHistSize = 0;

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
			history = (node*)malloc(sizeof(node));
			tail = history;
			history->prev = 0;
			history->next = 0;
			strncpy(history->command, buf, r-1);			
			curHistSize = 1;
		}
		else{
			node *com;
			com = (node*)malloc(sizeof(node));
			tail->next = com;
			com->prev = tail;
			tail = com;
			tail->next = 0;
			strncpy(com->command, buf, r-1);
			curHistSize += 1;
			
			// make sure our history doesn't grow beyond the max size
			if(curHistSize > MAX_HIST_LEN){
				node *toBeFreed = history;
				history = history->next;
				free(toBeFreed);
				curHistSize -= 1;
			}
		}
		
		// tokenize the string
		char *args[r];
		tokens = strtok(buf, " \n");
		int i = 0;
		bool runInBG = false;
		while((args[i] = tokens) != NULL){
			tokens = strtok(NULL, " \n");
			if(tokens != NULL && strcmp(tokens, "&") == 0){
				runInBG = true;
			}
			i++;
		}
		
		// if we read ar argument, pad the end of the array with a nul-byte
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
				if(tpid != childPID) processTerminated(tpid);
			}while(tpid != childPID);
			
			//cout << "Status of Child: " << childStatus << endl;
		}
		fflush(stdout);
		
		// clear the buffer
		resetBuf(buf);
	}
}

void printHistory(node *comHist){
	while(comHist != NULL){
		cout << comHist->command << endl;
		comHist = comHist->next;
	}
	//fflush(stdout);
}

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
* @returns			a pointer to the modified char array
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
    if( frontp != str )
    {
            while( *frontp ) *endp++ = *frontp++;
            *endp = '\0';
    }

    return str;
}

void processTerminated(int childPID){
	
	// don't print termination messages if the process didn't come from a 
	// forked background process
	if(childPID > 0)
		cout << "Child Process terminated: " << childPID << endl;
}

void resetBuf(char* buf){
	memset(buf, 0, sizeof(buf));
}


