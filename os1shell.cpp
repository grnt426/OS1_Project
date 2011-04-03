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

void printHistory(node *history);
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
		else if(r < 0){
			
			
		}
		
		// make sure we didn't read the contents of a signal
		if(donotread){
			donotread = false;
			continue;
		}
		
		// tokenize the string
		tokens = strtok(buf, " \n");
		
		// test the tokens (DEBUG ONLY)
		int count = 0;
		while(tokens != NULL){
			//cout << tokens << endl;
			tokens = strtok(NULL, " \n");
			count++;
		}
		
		// do some checking to make sure we got a real command
		
		
		
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
			strncpy(com->command, buf, MAX_BUF_SIZE);
			curHistSize += 1;
			
			// set new tail
			tail = com;
		}
		
		// check if we are running a shell-specific command
		if(strcmp(buf, "history") == 0){
			printHistory(history);
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
			
			// if we reached here, something bad happened
			cout << "Uh-oh...";
		}
		else{
			
			// this is done by the parent
			pid_t tpid;
			do{
				tpid = wait(&childStatus);
			}while(tpid != childPID);
			
			//cout << "Status of Child: " << childStatus << endl;
		}
	}
}

void printHistory(node *comhist){
	if(comhist == 0)
		return;
	cout << comhist->command << endl;
	printHistory(comhist->next);
	fflush(stdout);
}

void addCommand(node *history, node *tail, node *comnode){

}

void handler_function(int sig_id){
	if(sig_id == 2){
		cout << endl;
		printHistory(history);
	}
	donotread = true;
}

/*
* Given a size of the history, performs a check if the tail should be freed
* and if so returns the new tail (the old tail's previous node) 
*
* @returns				the current tail
*/
node* pruneHistory(node *historyTail, int size){
	
	// if full, return the history's tail's previous element
	if(size > MAX_HIST_LEN){
		node *tail = historyTail->prev;
		tail->next = 0;
		free(historyTail);
		return tail;
	}
	
	// otherwise just return our tail again
	return historyTail;
}


