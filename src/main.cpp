#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include "configreader.h"
#include "process.h"

// Shared data for all cores
typedef struct SchedulerData {
    std::mutex mutex;
    std::condition_variable condition;
    ScheduleAlgorithm algorithm;
    uint32_t context_switch;
    uint32_t time_slice;
    std::list<Process*> ready_queue;
    bool all_terminated;
} SchedulerData;

void coreRunProcesses(uint8_t core_id, SchedulerData *data);
int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex);
void clearOutput(int num_lines);
void printSchedulerStatistics(std::vector<Process*>& processes, uint32_t start);
uint32_t currentTime();
std::string processStateToString(Process::State state);

int main(int argc, char **argv)
{
    // ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(1);
    }

    // declare variables used throughout main
    int i;
    SchedulerData *shared_data;
    std::vector<Process*> processes;
    bool done = false;
    
    // read configuration file for scheduling simulation
    SchedulerConfig *config = readConfigFile(argv[1]);

    // store configuration parameters in shared data object
    uint8_t num_cores = config->cores;
    shared_data = new SchedulerData();
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;
    // create processes
    uint32_t start = currentTime();
    for (i = 0; i < config->num_processes; i++)
    {
        Process *p = new Process(config->processes[i], start);
        processes.push_back(p);
        if (p->getState() == Process::State::Ready)
        {
            shared_data->ready_queue.push_back(p);
        }
    }

    // free configuration data from memory
    deleteConfig(config);

    // launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores];
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data);
    }
    // main thread work goes here:
    int num_lines = 0;
    while (!(shared_data->all_terminated))
    {
        // clear output from previous iteration
        clearOutput(num_lines);


	{
	  std::lock_guard<std::mutex> lock(shared_data->mutex);
	  done = true;
	  
	  for(i = 0 ; i < processes.size(); i++){
	    //Check the burst counter and update processes
	    uint16_t burst_counter = processes[i]->getCurrentBurst();
	    processes[i]->updateProcess(currentTime());

	    //Do things if the process isn't terminated
	    if(processes[i]->getState() != Process::State::Terminated){
	      // start new processes at their appropriate start time
	      //@@Check through the NotStarted Processes
	      //@@See if time	      
	      if((currentTime() - start) >= processes[i]->getStartTime() && processes[i]->getState() == Process::State::NotStarted){
		processes[i]->setState(Process::State::Ready, currentTime());
		shared_data->ready_queue.push_back(processes[i]); // Had not started, now ready
	      }//if not started
	      
	      // determine when an I/O burst finishes and put the process back in the ready queue
	      //@@Check through the NotStarted Processes
	      //*Lauch a process
	      if(burst_counter != processes[i]->getCurrentBurst() && processes[i]->getState() == Process::State::IO){
		processes[i]->setState(Process::State::Ready, currentTime());
		//Add process to end of the queue
		shared_data->ready_queue.push_back(processes[i]); // was I/O, now ready
		
	      }//if in IO
	      
	      // determine if all processes are in the terminated state
	      done = false;//There was a process that is not terminated
	      
	    }//if not terminated
	  }//for each process
	  shared_data->all_terminated = done;
	  
	  // sort the ready queue (if needed - based on scheduling algorithm)
	  //@@ Only do it for SJF and PP	  
	  if(shared_data->algorithm == ScheduleAlgorithm::SJF){
	    shared_data->ready_queue.sort(SjfComparator());
	  }
	  else if(shared_data->algorithm == ScheduleAlgorithm::PP){
	    shared_data->ready_queue.sort(PpComparator());
	  }
	}//lock shared data

	
	
        // output process status table
	num_lines = printProcessOutput(processes, shared_data->mutex);

        //sleep 1/60th of a second
        usleep(16667);
    }//while something still runs

    //std::cout<<"All done in main thread"<<std::endl;
    // wait for threads to finish
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i].join();
    }
    
    // print final statistics
    //  - CPU utilization
    //  - Throughput
    //     - Average for first 50% of processes finished
    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time
    //  - Average waiting time
    printSchedulerStatistics(processes, start);


    // Clean up before quitting program
    processes.clear();

    return 0;
}//main()

void coreRunProcesses(uint8_t core_id, SchedulerData *shared_data)
{
  uint32_t context_switch;
  uint32_t time_slice;
  ScheduleAlgorithm algorithm;
  uint32_t cpu_burst_time;
  uint32_t start_cpu_time;
  Process *runningProcess;
  uint8_t burst_counter;
  
  context_switch = shared_data->context_switch;
  time_slice = shared_data->time_slice;
  algorithm = shared_data->algorithm;

  // while not all processes have terminated
  while(!shared_data->all_terminated){
    
    runningProcess = NULL;
    // While there is no process running on this core
    // and not all processes have terminated
    while(runningProcess == NULL && !shared_data->all_terminated){
      {//* CRITICAL SECTION GET FROM READ QUEUE
	
	std::lock_guard<std::mutex> lock(shared_data->mutex);
	if(shared_data->ready_queue.size() > 0){
	  // Work to be done by each core independent of the other cores
	  // Take process from front of ready queue and put it on this core and set it up:
	  //  - Get process at front of ready queue
	  runningProcess = shared_data->ready_queue.front();
	  //  - Remove the entry from the queue
	  shared_data->ready_queue.pop_front();
	  //*Update process core and state
	  runningProcess->setCpuCore(core_id);
	  runningProcess->updateProcess(currentTime());
	  runningProcess->setState(Process::State::Running, currentTime());
	  cpu_burst_time = runningProcess->getBurstTime();
	  burst_counter = runningProcess->getCurrentBurst();
	}
      }
    }
    if(!shared_data->all_terminated){
      start_cpu_time = currentTime();
      
      //  - Simulate the processes running until one of the following:
      //     - CPU burst time has elapsed
      //     - RR time slice has elapsed
      //     - Process preempted by higher priority process
      // Simulate by stalling for certain amount of milliseconds
      if(algorithm == ScheduleAlgorithm::FCFS){
	while(currentTime() - start_cpu_time < cpu_burst_time); // stall until process burst time is over
      }    
      else if(algorithm == ScheduleAlgorithm::RR){
	//If finishes or because of preemption
	// Process will stop executing on core and go back to ready queue if the time slice is smaller than burst time
	while(currentTime() - start_cpu_time < cpu_burst_time && currentTime() - start_cpu_time < time_slice);
      }
      else if(algorithm == ScheduleAlgorithm::SJF){
	while(currentTime() - start_cpu_time < cpu_burst_time);
      }
      else if(algorithm == ScheduleAlgorithm::PP){
	//{
	  //std::lock_guard<std::mutex> lock(shared_data->mutex);
	  //Check what the front process is on the thing
	  // Stall while burst and while highest priority in queue
	  //&& my priority is higher than the other people's priority
	  while(currentTime() - start_cpu_time < cpu_burst_time){
	  	std::lock_guard<std::mutex> lock(shared_data->mutex);
		if(shared_data->ready_queue.front() != NULL && runningProcess->getPriority() > shared_data->ready_queue.front()->getPriority()){
		  //std::cout<<"Preemting Priority: "<<unsigned(runningProcess->getPriority())<<"for higher priority: "<<unsigned(shared_data->ready_queue.front()->getPriority())<<std::endl; 
			break;
		}
		
	  //}
	  // When this is cut short because there is a higher priority process on the ready queue
	  // this process will go back on ready queue then that higher priority process will be pulled from front of sorted ready queue
	  
	}
      }
      
      
      //  - Place the process back in the appropriate queue
      //     - I/O queue if CPU burst finished (and process not finished)
      //     - Terminated if CPU burst finished and no more bursts remain
      //     - Ready queue if time slice elapsed or process was preempted
      {//* CRITICAL SECTION GET FROM READ QUEUE
	//*Update process core and state
	std::lock_guard<std::mutex> lock(shared_data->mutex);
	runningProcess->setCpuCore(-1); // take process off core
	runningProcess->updateProcess(currentTime()); // moves to next burst if more bursts and finished burst above
	//Terminate
	// Check if there are any more bursts for the process
	if(runningProcess->getCurrentBurst() >= runningProcess->getNumBursts()){
	    // if no more bursts then terminate
	  runningProcess->setState(Process::State::Terminated, currentTime());
	}
	//Finished CPU Burst
	//*
	// Process moves to I/O queue because it has more bursts, it's on a odd number burst
	else if(burst_counter != runningProcess->getCurrentBurst()){
	  runningProcess->setState(Process::State::IO, currentTime());
	}//*/
	//If preempted
	else{
	    // If the burst finished because it was preempted then put process back on ready queue because it is not done
	  runningProcess->setState(Process::State::Ready, currentTime());
	  shared_data->ready_queue.push_back(runningProcess);
	}
      }
    
      //  - Wait context switching time
      usleep(context_switch);
      //  * Repeat until all processes in terminated state
    }
  }//while ! done//*/
}

int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex)
{
    int i;
    int num_lines = 2;
    std::lock_guard<std::mutex> lock(mutex);
    printf("|   PID | Priority |      State | Core | Turn Time | Wait Time | CPU Time | Remain Time |\n");
    printf("+-------+----------+------------+------+-----------+-----------+----------+-------------+\n");
    for (i = 0; i < processes.size(); i++)
    {
        if (processes[i]->getState() != Process::State::NotStarted)
        {
            uint16_t pid = processes[i]->getPid();
            uint8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double turn_time = processes[i]->getTurnaroundTime();
            double wait_time = processes[i]->getWaitTime();
            double cpu_time = processes[i]->getCpuTime();
            double remain_time = processes[i]->getRemainingTime();
            printf("| %5u | %8u | %10s | %4s | %9.1lf | %9.1lf | %8.1lf | %11.1lf |\n", 
                   pid, priority, process_state.c_str(), cpu_core.c_str(), turn_time, 
                   wait_time, cpu_time, remain_time);
            num_lines++;
        }
    }
    return num_lines;
}

void clearOutput(int num_lines)
{
    int i;
    for (i = 0; i < num_lines; i++)
    {
        fputs("\033[A\033[2K", stdout);
    }
    rewind(stdout);
    fflush(stdout);
}

void printSchedulerStatistics(std::vector<Process*>& processes, uint32_t start)
{
	uint32_t end = currentTime();
	uint32_t total_time = end - start;
	double total_turn_time = 0;
	double total_wait_time = 0;
	double total_cpu_time = 0;
	// Throughput - number of processes executed by the CPU in a given amount of time
	// total time / # processes
	int i;
	for (i = 0; i < processes.size(); i++)
	{
		total_turn_time += processes[i]->getTurnaroundTime();
        total_wait_time += processes[i]->getWaitTime();
        total_cpu_time += processes[i]->getCpuTime();
	}
    printf("\nScheduler Statistics: \n");
    printf("Total time: %f \n", total_time/1000.0);
    printf("Total turn time: %f \n", total_turn_time);
    printf("CPU utilization: %f \n", total_time/total_cpu_time);
    printf("Throughput:\n");
    printf("\tAverage for first 50%% of processes finished: \n");
    printf("\tAverage for second 50%% of processes finished: \n");
    printf("\tOverall average: %f \n", total_time/processes.size());
    printf("Average turnaround time: %f \n", total_turn_time/processes.size());
    printf("Average waiting time: %f \n", total_wait_time/processes.size());
}

uint32_t currentTime()
{
    uint32_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    return ms;
}

std::string processStateToString(Process::State state)
{
    std::string str;
    switch (state)
    {
        case Process::State::NotStarted:
            str = "not started";
            break;
        case Process::State::Ready:
            str = "ready";
            break;
        case Process::State::Running:
            str = "running";
            break;
        case Process::State::IO:
            str = "i/o";
            break;
        case Process::State::Terminated:
            str = "terminated";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}
