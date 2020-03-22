void printSchedulerStatistics(std::vector<Process *> &processes, uint32_t start) {
    uint32_t end = currentTime();
    uint32_t total_time = end - start;
    double total_turn_time = 0;
    double total_wait_time = 0;
    double total_cpu_time = 0;
    int i;
    for (i = 0; i < processes.size(); i++) {
        total_turn_time += processes[i]->getTurnaroundTime();
        total_wait_time += processes[i]->getWaitTime();
        total_cpu_time += processes[i]->getCpuTime();
    }
    printf("\nScheduler Statistics: \n");
    printf("(test) Total time: %f \n", total_time / 1000.0);
    printf("(test) Total turn time: %f \n", total_turn_time);
    printf("CPU utilization: %f%% \n", (total_cpu_time / total_turn_time) * 100);
    printf("Throughput:\n");
    // Throughput is the number of processes executed by the CPU in a given amount of time
    printf("\tAverage for first 50%% of processes finished: \n");
    printf("\tAverage for second 50%% of processes finished: \n");
    printf("\tOverall average: \n");
    printf("Average turnaround time: %f seconds\n", total_turn_time / processes.size());
    printf("Average waiting time: %f seconds\n", total_wait_time / processes.size());
}