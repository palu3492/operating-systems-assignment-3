#include "process.h"

// Process class methods
Process::Process(ProcessDetails details, uint32_t current_time)
{
    int i;
    pid = details.pid;
    start_time = details.start_time;
    num_bursts = details.num_bursts;
    current_burst = 0;
    burst_times = new uint32_t[num_bursts];
    for (i = 0; i < num_bursts; i++)
    {
        burst_times[i] = details.burst_times[i];
    }
    priority = details.priority;
    state = (start_time == 0) ? State::Ready : State::NotStarted;
    if (state == State::Ready)
    {
      launch_time = current_time;
    }
    core = -1;
    turn_time = 0;
    wait_time = 0;
    cpu_time = 0;
    remain_time = 0;
    last_updated_time = current_time;//Added to keep track of time slices
    for (i = 0; i < num_bursts; i+=2)
    {
        remain_time += burst_times[i];
    }
}

Process::~Process()
{
    delete[] burst_times;
}

uint16_t Process::getPid() const
{
    return pid;
}

uint32_t Process::getStartTime() const 
{
    return start_time;
}

uint16_t Process::getNumBursts() const
{
    return num_bursts;
}

uint8_t Process::getPriority() const 
{
    return priority;
}

Process::State Process::getState() const 
{
    return state;
}

int8_t Process::getCpuCore() const 
{
    return core;
}

double Process::getTurnaroundTime() const 
{
    return (double)turn_time / 1000.0;
}

double Process::getWaitTime() const 
{
    return (double)wait_time / 1000.0;
}

double Process::getCpuTime() const 
{
    return (double)cpu_time / 1000.0;
}

double Process::getRemainingTime() const 
{
    return (double)remain_time / 1000.0;
}

int16_t Process::getCurrentBurst() const
{
    return current_burst;
}

uint32_t Process::getBurstTime() const
{
    return burst_times[current_burst];
}


void Process::setState(State new_state, uint32_t current_time)
{
    if (state == State::NotStarted && new_state == State::Ready)
    {
        launch_time = current_time;
	last_updated_time = launch_time;
    }

    state = new_state;
}

void Process::setCpuCore(int8_t core_num)
{
    core = core_num;
}

void Process::updateProcess(uint32_t current_time)
{
    // use `current_time` to update turnaround time, wait time, burst times,
    // cpu time, and remaining time
  //Alwats update the turn_time
  uint32_t time_spent = current_time - last_updated_time;
  if(state != State::Terminated){
    turn_time = current_time - launch_time;
  }
  //We were in the ready moving to cpu
  if(state == State::Ready){
    if(time_spent > 0){
      wait_time = wait_time + time_spent;

    }
    //    if(wait_time < 0){
    //  std::cout<<"Wait time: "<<wait_time<<" time_spent: "<<time_spent<<" last_updated_time: "<<last_updated_time<<" current_time: "<<current_time<<std::endl;
    //}
  }
  //we are moving off the CPU
  if(state == State::Running){
    cpu_time = cpu_time + time_spent;
    remain_time = remain_time - time_spent;
    if(time_spent >= getBurstTime()){
      current_burst++;
    }
    else{
      updateBurstTime(current_burst,getBurstTime() - time_spent);
    }
  }
  //If we are moving off the IO queue
  if(state == State::IO){
    if(time_spent >= getBurstTime()){
      current_burst++;
    }
    else{
      updateBurstTime(current_burst,getBurstTime() - time_spent);
    }
  }
  //Keep track of the last time we updated
  last_updated_time = current_time;
  
}

void Process::updateBurstTime(int burst_idx, uint32_t new_time)
{
    burst_times[burst_idx] = new_time;
}


// Comparator methods: used in std::list sort() method
// No comparator needed for FCFS or RR (ready queue never sorted)

// SJF - comparator for sorting read queue based on shortest remaining CPU time
bool SjfComparator::operator ()(const Process *p1, const Process *p2)
{
    // your code here!
  bool result = false;
  if(p1->getRemainingTime() != p2->getRemainingTime()){
    result = (p1->getRemainingTime() < p2->getRemainingTime());
  }
  return result;
}

// PP - comparator for sorting read queue based on priority
bool PpComparator::operator ()(const Process *p1, const Process *p2)
{
    // your code here!
  bool result = false;
  if(p1->getPriority() != p2->getPriority()){
     result = (p1->getPriority() < p2->getPriority());
  }
  return result;
}
