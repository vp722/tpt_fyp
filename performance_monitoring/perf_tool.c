
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <stdint.h>


// System call wrapper for perf_event_open
static int perf_event_open(struct perf_event_attr *attr, pid_t pid,
    int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

// Basic counter structure
struct perf_counter {
    int fd;
    uint64_t id;
    const char *name;
    uint64_t value;
};

// Initialize a performance counter
int init_counter(struct perf_counter *counter, 
    uint32_t type, uint64_t config, const char *name) {
    struct perf_event_attr attr = {
        .type = type,
        .size = sizeof(struct perf_event_attr),
        .config = config,
        .disabled = 1,
        .exclude_kernel = 1,
        .exclude_hv = 1
    };

    counter->fd = perf_event_open(&attr, 0, -1, -1, 0);
    if (counter->fd < 0) {
        fprintf(stderr, "Error creating counter %s: %s\n",
        name, strerror(errno));
        return -1;
    }

    counter->name = name;
    return 0;
}

void run_and_measure(const char *filename) {
    // This function should run the program and measure its performance
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        char *args[] = {filename, NULL};
        execv(filename, args);
        perror("execv failed");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else {

        // Parent process
        
        printf("Measuring performance for %s...\n", filename);

        struct perf_counter counters[2];

        if (init_counter(&counters[0], PERF_TYPE_HARDWARE, 
            PERF_COUNT_HW_CPU_CYCLES, "CPU Cycles") < 0) {
            exit(EXIT_FAILURE);
        }
        if (init_counter(&counters[1], PERF_TYPE_HARDWARE, 
            PERF_COUNT_HW_INSTRUCTIONS, "Instructions") < 0) {
            exit(EXIT_FAILURE);
        }

        // Start the counters
        for (int i = 0; i < 2; i++) {
            ioctl(counters[i].fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(counters[i].fd, PERF_EVENT_IOC_ENABLE, 0);
        }
        // Wait for the child process to finish 
        int status;
        waitpid(pid, &status, 0);
        // Stop the counters
        for (int i = 0; i < 2; i++) {
            ioctl(counters[i].fd, PERF_EVENT_IOC_DISABLE, 0);
            read(counters[i].fd, &counters[i].value, sizeof(uint64_t));
            printf("%s: %llu\n", counters[i].name, counters[i].value);
            // Close the file descriptor
            close(counters[i].fd);
        }
    }
}



int main(int argc, char *argv[])   
{
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // the file is a program to run -> run it in another function 
    // and pass the filename to it

    run_and_measure(argv[1]);


    return EXIT_SUCCESS;
}