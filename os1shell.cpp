/**
*
* File: 		os1shell.cpp
*
* Author: 		Grant Kurtz
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

void printHistory(node *history, int count);
node* pruneHistory(node *historyTail, int size);
void addCommand(node *history, node *tail, node *comnode);
void handler_function(int sig_id);

int main(int argc, char *argv[]){
		
	// vars
	char buf[MAX_BUF_SIZE-1];
	char* tokens;
	bool alive;
	int curHistSize = 0;

	struct sigaction signal_action;
	signal_action.sa_handler = handler_function;
	signal_action.sa_flags = 0;
	sigemptyset(&signal_action.sa_mask);
	sigaction(SIGINT, &signal_action, NULL);
	
	
	// initialization setup
	alive = true;
	
	while(alive){
		
		// provide prompt and wait for input
		write(0, "OS1Shell -> ", 12);
		int r = read(0, buf, MAX_BUF_SIZE);
		
		// ^D received
		if(r == 0){
			cout << endl;
			return 0;
		}
		else if(r == 1){
			fflush(stdout);
			continue;
		}
		else if(r < 0){
			
			
		}
		
		// make sure we didn't read the contents of a signal
		if(donotread){
			donotread = false;
			continue;
		}
		
		// tokenize the string
		tokens = strtok(buf, " \n");
		
		// do some checking to make sure we got a real command
		if(strcmp(tokens, "") == 0){
			fflush(stdout);
		
			// clear the buffer
			memset(buf, 0, sizeof(buf));
			continue;
		}
		
		
		// alright, the command is good, add to our history for recall
		if(curHistSize == 0){
		
			history = (node*)malloc(sizeof(node));
			tail = history;
			history->prev = 0;
			history->next = 0;
			strncpy(history->command, buf, MAX_BUF_SIZE);			
			curHistSize = 1;
		}
		else{
			
			node *com;
			com = (node*)malloc(sizeof(node));
			tail->next = com;
			com->prev = tail;
			tail = com;
			tail->next = 0;
			strncpy(com->command, buf, MAX_BUF_SIZE);
			curHistSize += 1;
			
			// make sure our history doesn't grow beyond the max size
			if(curHistSize > MAX_HIST_LEN){
				node *toBeFreed = history;
				history = history->next;
				free(toBeFreed);
				curHistSize -= 1;
			}
			
			
		}
		
		// check if we are running a shell-specific command
		if(strcmp(buf, "history") == 0){
			printHistory(history, 0);
			continue;
		}
		
		
		// Run the command
		pid_t childPID;
		int childStatus;
		
		childPID = fork();
		
		// If zero, then this is the child running
		if(childPID == 0){
			
			// execute the command
			execvp(buf, argv);
			
			// if we reached here, the command was invalid
			cout << "Bad command!" << endl;
		}
		else{
			
			// this is done by the parent
			pid_t tpid;
			do{
				tpid = wait(&childStatus);
			}while(tpid != childPID);
			
			//cout << "Status of Child: " << childStatus << endl;
		}
		fflush(stdout);
		
		// clear the buffer
		memset(buf, 0, sizeof(buf));
	}
}

void printHistory(node *comHist, int count){
	if(comHist == 0 || count == 10)
		return;
	cout << comHist->command << endl;
	printHistory(comHist->next, (count+1));
	fflush(stdout);
}

void handler_function(int sig_id){
	if(sig_id == 2){
		cout << endl;
		printHistory(history, 0);
	}
	donotread = true;
}

                                                           