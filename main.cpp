#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


using namespace std;



//Basic struct used to store information for all processes stopped or running in background
struct process{
	pid_t pid;
	string data[21];
	bool running;
};

//Global Variables
int runningPrograms = 0; //number of running programs
struct process *allprocces= new process[200]; // hold all the processes, maximum limit is 200
bool ended = false; //if the user is exiting the shell
pid_t fg_pid =-1; //current foreground process pid
string fgdesc[21]; //current foreground process arguements
bool cont = false; // continue the busy loop



//Create a process object given inputs, functionally similar to a constructor for a class
process createprocess(pid_t pid, string data[], bool nrunning)
{
	process obj;
 	obj.pid = pid;
	for(int c = 0; c<21; c++)
	{
		obj.data[c] = data[c];
	}
	obj.running = nrunning;

	return obj;
}



//Add a process to allprocesses list
void addprocess(pid_t pidnum, string desc[], bool running)
{
	allprocces[runningPrograms] = createprocess(pidnum, desc, running);
	runningPrograms++;

}

//Remove a process from total process list, in case of wrong pids, to prevent errors it will not do anything
//If a process is found, everything right of the index will be moved to the left and the last entry will be turned to null
void removeprocess(pid_t pidnum)
{
	int index;
	bool worked = false;
	
	for(int c = 0; c<runningPrograms; c++)
		if(pidnum==allprocces[c].pid)
		{
			index = c;
			worked = true;
		}
	if(worked){
		for(int c = index; c<runningPrograms-1; c++)
				allprocces[c] = allprocces[c+1];

		allprocces[runningPrograms].pid = 0;
		allprocces[runningPrograms].running =  false;
		
		for(int c =0; c<21; c++)
			allprocces[runningPrograms].data[c]="";
		runningPrograms--;
	}
}

//Program for running fg

//Steps:
//Read all the arguments, check for valid length of arugments
//fork - error check if fork failed
//Move child process outside of foreground group and run execvp, if execvp fails, then show error and exit program for the child
//The parent runs a sleep busy loop that is altered only by the sig_child_handler
void fgrun(char* argu)
{
	char* p = strtok(argu, " \t"); 
	
	char* args[] ={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};		
	
	if(p==0)
	{
		printf("Error: No file name argument found\n");
		return;
	}

	args[0] = p;
	int argCounter = 1;
	bool worked = true;

	while (p = strtok(NULL, " \t"))
	{
		if (argCounter == 21)
		{
			printf ("Error: Cannot handle so many arguments!\n");
			worked = false;
		}
		args[argCounter] = p;
		argCounter++;
			

	}
	if(runningPrograms ==199)
	{
		printf("Maximun number of programs reached, kill or wait for a program to finish!");
		return;
	}


	if (worked)
	{
		fg_pid = fork();
		
		for(int c = 0; c<21; c++)
			if(args[c]!=0)
			fgdesc[c] =string(args[c]);
			else
			fgdesc[c]="";

		if (fg_pid == 0)
		{
			setpgid(0,0);
			
			if (execvp(args[0], args) <= 0)
			{//if a child process fails to run it must be exited
					printf ("Error: File could not be found or arguments had some error\n");
					kill(getpid(), SIGTERM); //SIGTERM to make sure the child is dead and prevent any issues even though the exit is there
					exit(EXIT_SUCCESS);
					
			}
				
			
		}
		else if(fg_pid>0)
		{
			//sleep busy loop to keep the parent busy till the SIGCHILD signal
			while(!cont)
			{
				sleep(5);
		
			};
			cont = false;
			fg_pid = -1;
			for(int c = 0; c<21; c++)
				fgdesc[c]="";		
		}
		else{
			printf("Error: Fork could not be created\n");
		}	
	}
	
}

//Function for background processes
//Similar to foreground processes but does not have a busy loop in the parent after the fork, but add the process to the overall list instead
void bgrun(char* argu)
{
		char* p = strtok(argu, " \t"); 
		char* args[] ={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

		if(p==0)
		{
			printf("Error: No file name argument found\n");
			return;
		}

	args[0] = p;
	int argsCounter = 1;
	bool worked = true;
	while (p = strtok(NULL, " \t"))
	{
		if (argsCounter == 21)
		{
			printf("Error: Too many arguments!\n");
		 	return;
		}
		args[argsCounter] = p;
		argsCounter++;
	}
	if(runningPrograms ==199)
	{
		printf("Maximun number of programs reached, kill or wait for a program to finish!");
		return;
	}



	pid_t pid = fork();
		if (pid == 0)
		{
			setpgid(0, 0);
			if (execvp(args[0], args) <= 0)
			{
					//similar reasoning as the fg processes
					printf("Error: File could not be found or arguments had some error\n");
					kill(getpid(), SIGTERM);
					exit(EXIT_SUCCESS);
			}
		}
		else if(pid>0)
		{
			string temp[21];

			for(int c = 0; c<21; c++)
			if(args[c]!=0)
				temp[c]=string(args[c]);
			else
				temp[c]="";
			//process is added to overall list	
			addprocess(pid,temp, true);
		}
			
}


//SigChild handler
//The handler manages ending of bg and fg programs but not ending of programs when the shell is exiting because of exiting needing to clear all possible proccesses
//Shows exit status, clears busy loop if needed and does not interact with stopped programs
void sig_child_handler(int sig){

	if(!ended){

	pid_t p;
    int status;
	while((p=waitpid(-1, &status, WNOHANG|WUNTRACED))>0)
  	{
		  if(!WIFSTOPPED(status)) //if a process stopped or terminated, Only terminated programs should pass through the if condition
		  {
			  //if a background process or foreground process was terminated
			if(p!=fg_pid)
			{ 	
					removeprocess(p);
					if(WIFEXITED(status))
					printf("Process %d has completed normally\n", p);
					else
					printf("Process %d has ended not normally \n", p);
			}
			else
			{
				if(WIFEXITED(status))
					printf("Process %d has completed normally\n", p);
				else
					printf("Process %d has been terminated forcefully\n", p);
				cont = true; //end the busy loop
			}
				
		  }
  	}
		
	}
}


//List of all programs in the shell
void list()
{
	int argsCounter = 0;

	if(runningPrograms==0)
	printf("No processes are currently running or stopped\n");

	else
	for(int c = 0; c<runningPrograms; c++)
	{
		argsCounter=0;
		printf("Process %d:", allprocces[c].pid);
		while(allprocces[c].data[argsCounter]!="")
		{
			cout<<allprocces[c].data[argsCounter]<<" ";
			argsCounter++;

			if(argsCounter==21)
				break;
		}
		
			if(allprocces[c].running)
			printf(" status: Running");
	
		else
			printf(" status: Stopped");

	printf("\n");
	}

}


//Shell exit program
//SIGKIll is used to be able to kill stopped programs too, not just running programs
//A while loop is used inside to make sure all pids are caught and killed, the extra precaution is because of the sigkill command possible taking some time to run, so be safe, using a while loop
void end(){
	  int status;
	ended = true;
    for(int c = 0; c<runningPrograms; c++){
      
        kill(allprocces[c].pid, SIGKILL);
        pid_t pid = waitpid(-1, &status, WNOHANG);
        while(!pid)
            pid = waitpid(-1, &status, WNOHANG);
		printf("Process %d has been ended\n", allprocces[c].pid);
    }
	while(waitpid(-1, &status, WNOHANG|WUNTRACED) >0);
	
}

//stop and kill handlers for SIGTSTP and SIGINT respectively
void killfg(int sig){

	if(fg_pid!=-1)
	{
		kill(fg_pid, SIGTERM);
	}
	
}
void stop(int sig){
	
	if(fg_pid!=-1)
	{
 		addprocess(fg_pid,fgdesc,false); //add the stopped process to the overall list
		printf("Process %d has been stopped\n", fg_pid);
		kill(fg_pid, SIGTSTP); // forward the stop signal to the child
		cont = true; //stop the busy loop
	}
}

//code for continue, creates busy loop to stop execution
//load the values from the array to the fg variables and remove it from the list
//send the continue signal and create a busy loop
void contin(int index)
{

	fg_pid= allprocces[index].pid;
	for(int c = 0; c<21; c++)
		fgdesc[c]=allprocces[index].data[c];
	printf("Process %d has been continued\n", allprocces[index].pid);
	
	cont = false;
	kill(allprocces[index].pid, SIGCONT);
	removeprocess(allprocces[index].pid);
	while(!cont)
	{
		sleep(5);
	};
	cont = false;
	fg_pid = -1;
}


//Main code where the main while loop runs and the signal handlers are handled
//Printf is used other than places where string needs to be printed
//input handling is managed here and other steps
int main()
{
	signal (SIGCHLD, &sig_child_handler);
	signal (SIGINT, &killfg);
	signal (SIGTSTP, &stop );
	printf("Welcome to the shell, type help to see commands\n");
	string command="";

	do {
		command="";
		printf("sh >");
		char input[2048] ={0};
		cin.getline(input, 2048, '\n');
		char* p = strtok(&input[0], " \t");
	
		if(p!=0)
		command = string(p);

		if(p==0 || command[0]==' ' || strlen(command.c_str())<2  )
		{
			printf("Error: Command not found or no characters entered, type help to learn about commands\n");
		}
		else if (command == "bg")
		{
			bgrun(&input[strlen(command.c_str())+1]);
		}
		else if(command== "list")
		{
			list();
		}
		else if (command == "fg")
		{	fgrun(&input[strlen(command.c_str())+1]);
			
		}
		else if(command=="cont")
		{//error checking for cont is done here before it's passed to the contin method
			do{
				int num = 0;
				p =  strtok(NULL, " \t");
				
				if(p==0)
				{
					printf("Error: Did not enter any pid!\n");
						break;
				}
				num = atoi(p);
				bool worked = false;

				for(int c = 0; c<runningPrograms; c++)
				if(allprocces[c].pid==num && !allprocces[c].running)
				{
					worked = true;
					allprocces[c].running=true;
					contin(c);
					break;
				}
				if(!worked)
				printf("Error: Process is already running or not found!\n");

				break;
			}while(true);
		}
		else if(command=="help")
		{
			printf("\nThe shell has 6 commands\n\n");
			printf("Type fg [Filename] [Arguments] to run a foreground process\n");
			printf("A sample command is fg ./test hello 2 5, the program can handle up to 20 inputs not including file name and any length inputs\n");
			printf("During an fg command, the shell will not accept any inputs, to stop a foreground process do Ctrl+Z, stopped processes can be restarted, and Ctrl+C to kill a foreground process.\n\n");

			printf("Type bg [Filename] [Arguments] to run a background process\n");
			printf("During background processes the shell can take input at anytime, even if the sh > gets disrupted due to background process output\n");
			printf("A sample command is bg ./test hello 2 5, background process can be manually killed but not stopped, and can be monitored using 'list'\n\n");

			printf("Type list to see all running and stopped process\n\n");

			printf("Type cont [pid] to continue a stopped process. It will run as a foreground process and shell cannot accept input in the meanwhile\n");
			printf("Example is cont 12341\n\n");

			printf("Type kill [pid] to kill a running background process, only running background process can be killed, stopped processes cannot be kiled\n");
			printf("Example is kill 12341\n\n");

			printf("Type exit to exit the shell, the shell will close all background processes\n\n");
		}
		else if(command=="kill")
		{//error checking here is identical to cont before the signal is sent for kill
			do{
			int num = 0;
			p =  strtok(NULL, " \t");

			if(p==0)
			{
				printf("Error: Did not enter any pid!\n");
					break;
			}


			num = atoi(p);
			bool worked = false;
			for(int c = 0; c<runningPrograms; c++)
			if(allprocces[c].pid==num && allprocces[c].running)
			{
				worked = true;
				allprocces[c].running=true;
				kill(allprocces[c].pid, SIGTERM);
				break;
			}

			if(!worked)
			printf("Error: Process is not found or is stopped!\n");

			break;
			}while(true);
		}
		else
		{
			if(command!="exit")
			cout<<"Error: "<<command<<" is not a command, type help to learn about commands\n";
		}
	}
	while(command != "exit");
	end();



}
