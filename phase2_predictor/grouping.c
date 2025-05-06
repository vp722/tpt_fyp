// this is a real time profiling tool - that collects metrics during the execution of the program
// and makes a decision to enable TPT (one way) for the rest of the execution
// this is a sampling based profiling tool extention for perf

#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#define COUNTER_COUNT 7
#define SAMPLING_INTERVAL_SEC 1
#define CPI_THRESHOLD 1.0
#define MAX_COUNTERS_PER_GROUP 4 
#define MAX_GROUPS 10

struct perf_counter {
    int fd;
    struct perf_event_attr attr;
    const char *name;
    uint64_t value;
};

struct counter_group {
    struct perf_counter counters[MAX_COUNTERS_PER_GROUP];
    int num_counters;
    uint64_t last_values[MAX_COUNTERS_PER_GROUP];
    bool complted; 
}; 

// global state 
static struct counter_group groups[MAX_GROUPS];
static int current_group = 0;
static int total_groups = 0;

static int perf_event_open(struct perf_event_attr *attr, pid_t pid,
                           int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

void init_counter(struct perf_counter *counter, uint32_t type, 
                uint64_t config, const char *name, pid_t pid, int group_fd) {
    memset(&counter->attr, 0, sizeof(counter->attr));
    counter->attr.type = type;
    counter->attr.size = sizeof(counter->attr);
    counter->attr.config = config;
    counter->attr.disabled = 1;
    counter->attr.inherit = 1;
    counter->attr.exclude_kernel = 1;
    counter->attr.exclude_hv = 1;
    counter->attr.exclude_idle = 1;

    counter->fd = perf_event_open(&counter->attr, pid, -1, group_fd, 0);
    if (counter->fd < 0) {
        fprintf(stderr, "Error creating %s: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    counter->name = name;
}

void init_counters(pid_t pid) {
    // set total groups 
    total_groups = 2; 
    groups[0].num_counters = 4;
    groups[1].num_counters = 3;
    init_counter(&groups[0].counters[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cycles", pid, -1);
    init_counter(&groups[0].counters[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions", pid, -1);
    init_counter(&groups[0].counters[2], PERF_TYPE_HW_CACHE, 
        PERF_COUNT_HW_CACHE_DTLB | 
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
        "dtlb_load_misses", pid, -1);
    
    init_counter(&groups[0].counters[3], PERF_TYPE_HW_CACHE,
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
        "dtlb_store_misses", pid, -1);
    
    init_counter(&groups[1].counters[0], PERF_TYPE_HW_CACHE,
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
        "dtlb_loads", pid, -1);
    
    init_counter(&groups[1].counters[1], PERF_TYPE_HW_CACHE,
        PERF_COUNT_HW_CACHE_DTLB |
        (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
        "dtlb_stores", pid, -1);
    
    init_counter(&groups[1].counters[2], PERF_TYPE_RAW, 0x01f2, "ept_walk_cycles", pid, -1);

    // initize that the group is not completed
    for (int i = 0; i < total_groups; i++) {
        groups[i].complted = false;
    }

}

void rotate_counter_groups() {
    // Disable previous group
    if (groups[current_group].complted) {
        for (int i = 0; i < groups[current_group].num_counters; i++) {
            ioctl(groups[current_group].counters[i].fd, PERF_EVENT_IOC_DISABLE, 0);
        }
        groups[current_group].complted = false;
    }

    // Move to next group
    current_group = (current_group + 1) % total_groups;

    // Enable new group
    for (int i = 0; i < groups[current_group].num_counters; i++) {
        ioctl(groups[current_group].counters[i].fd, PERF_EVENT_IOC_ENABLE, 0);
    }
    groups[current_group].complted = true;
}

void sample_counters(struct perf_counter counters[]) {
    for (int i = 0; i < COUNTER_COUNT; i++) {
        ssize_t bytes_read = read(counters[i].fd, &counters[i].value, sizeof(uint64_t));
        if (bytes_read != sizeof(uint64_t)) {
            fprintf(stderr, "In sasmple counters: Failed to read %s: %s\n", counters[i].name, strerror(errno));
            counters[i].value = 0;
        }
	// ioctl(counters[i].fd, PERF_EVENT_IOC_RESET, 0); // need to reset counters or not. 
    }
}


void sample_active_group() {
    struct counter_group *active = &groups[current_group];
    
    for (int i = 0; i < active->num_counters; i++) {
        uint64_t new_value;
        read(active->counters[i].fd, &new_value, sizeof(uint64_t));
        
        // Calculate delta since last sample of this group
        uint64_t delta = new_value - active->last_values[i];
        active->last_values[i] = new_value;
        
        // Store/process deltas
        printf("[Group %d] %s: %lu (+%lu)\n", 
              current_group, 
              active->counters[i].name,
              new_value,
              delta);
    }
}

int should_enable_tpt(struct perf_counter counters[]) {
    uint64_t cycles = counters[0].value;
    uint64_t instructions = counters[1].value;

    printf("Cycles: %lu, Instructions: %lu\n", cycles, instructions);

    if (instructions == 0) return 0;

    double cycles_per_instruction = (double)cycles / instructions;
    printf("Cycles per Instruction: %.2f\n", cycles_per_instruction);

    return cycles_per_instruction > CPI_THRESHOLD;
}

void run_executable(const char *program, char *const argv[]) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe creation failed");
        exit(EXIT_FAILURE);
    }

    struct perf_counter counters[COUNTER_COUNT];
    pid_t pid = fork();

    if (pid == 0) {
        close(pipefd[1]);
        char dummy;
        read(pipefd[0], &dummy, 1);
        close(pipefd[0]);

        execvp(program, argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        close(pipefd[0]);

        init_counters(pid);

        for (int i = 0; i < COUNTER_COUNT; i++) {
            if (ioctl(counters[i].fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
                fprintf(stderr, "Failed to enable %s: %s\n", counters[i].name, strerror(errno));
            }
        }

        write(pipefd[1], "G", 1);
        close(pipefd[1]);

        int status;
        time_t last_sample_time = time(NULL);

        while (waitpid(pid, &status, WNOHANG) == 0) {
            time_t current_time = time(NULL);
            if (current_time - last_sample_time >= SAMPLING_INTERVAL_SEC) {
                rotate_counter_groups();
                sample_active_group();
                // sample_counters(counters);

                if (current_group == total_groups - 1) {
                    if (should_enable_tpt(counters)) {
                        printf("=========== ACTION : ENABLE TPT ===========\n");
                    }
                }

                last_sample_time = current_time;
            }

            struct timespec sleep_time = { .tv_sec = 0, .tv_nsec = 100000000 }; // 100ms
            nanosleep(&sleep_time, NULL);
        }

        // Cleanup counters
        for (int i = 0; i < COUNTER_COUNT; i++) {
            ioctl(counters[i].fd, PERF_EVENT_IOC_DISABLE, 0);
            close(counters[i].fd);
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Program execution failed.\n");
        }
    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    run_executable(argv[1], &argv[1]);
    return EXIT_SUCCESS;
}
