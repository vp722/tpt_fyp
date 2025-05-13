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
#include <unistd.h>
#include <stdbool.h>

#define COUNTER_COUNT 7
#define SAMPLING_INTERVAL_SEC 1
#define AVG_WALK_CYCLES 25 // 20 cycles

struct perf_counter {
    int fd;
    struct perf_event_attr attr;
    const char *name;
    uint64_t value;
    uint64_t prev_value;
    uint64_t delta;
};

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
    counter->value = 0;
    counter->prev_value = 0;
    counter->delta = 0;
}

void init_counters(struct perf_counter counters[], pid_t pid) {
    // Set cycles as group leader
    init_counter(&counters[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cycles", pid, -1);
//    int group_fd = counters[0].fd;

    init_counter(&counters[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions", pid, -1);

    init_counter(&counters[2], PERF_TYPE_RAW, 0x1008, "dtlb_load_misses_walk_duration", pid, -1);
    init_counter(&counters[3], PERF_TYPE_RAW, 0x1049,"dtlb_store_misses_walk_duration", pid, -1);
    init_counter(&counters[4], PERF_TYPE_RAW, 0x0e08, "dtlb_load_misses.walk_completed", pid, -1); 
    init_counter(&counters[5], PERF_TYPE_RAW, 0x0e49, "dtlb_store_misses.walk_completed", pid, -1); 

    // init_counter(&counters[2], PERF_TYPE_HW_CACHE, 
    //     PERF_COUNT_HW_CACHE_DTLB | 
    //     (PERF_COUNT_HW_CACHE_OP_READ << 8) |
    //     (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
    //     "dtlb_load_misses", pid, -1);
    
    // init_counter(&counters[3], PERF_TYPE_HW_CACHE,
    //     PERF_COUNT_HW_CACHE_DTLB |
    //     (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
    //     (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
    //     "dtlb_store_misses", pid, -1);
    
    // init_counter(&counters[4], PERF_TYPE_HW_CACHE,
    //     PERF_COUNT_HW_CACHE_DTLB |
    //     (PERF_COUNT_HW_CACHE_OP_READ << 8) |
    //     (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
    //     "dtlb_loads", pid, -1);
    
    // init_counter(&counters[5], PERF_TYPE_HW_CACHE,
    //     PERF_COUNT_HW_CACHE_DTLB |
    //     (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
    //     (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
    //     "dtlb_stores", pid, -1);
    
   init_counter(&counters[6], PERF_TYPE_RAW, 0x104f, "ept_walk_cycles", pid, -1);
}


uint64_t get_rss_in_bytes(pid_t pid) {
    char filename[256];
    snprintf(filename, sizeof(filename), "/proc/%d/statm", pid);
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return 0;
    }

    uint64_t rss_value;
    if (fscanf(file, "%*s %lu", &rss_value) != 1) {
        perror("fscanf");
        fclose(file);
        return 0;
    }
    fclose(file);
    // Convert pages to bytes   
    return rss_value * getpagesize();
}

void sample_counters(struct perf_counter counters[]) {
    for (int i = 0; i < COUNTER_COUNT; i++) {
        counters[i].prev_value = counters[i].value;
        ssize_t bytes_read = read(counters[i].fd, &counters[i].value, sizeof(uint64_t));
        if (bytes_read != sizeof(uint64_t)) {
            fprintf(stderr, "In sasmple counters: Failed to read %s: %s\n", counters[i].name, strerror(errno));
            counters[i].value = 0;
        }

        // Calculate delta
        counters[i].delta = counters[i].value - counters[i].prev_value;

//	printf("%s: %lu\n", counters[i].name, counters[i].value);
//	ioctl(counters[i].fd, PERF_EVENT_IOC_RESET, 0);
    }
}

int should_enable_tpt(struct perf_counter counters[], pid_t pid) {
    uint64_t cycles = counters[0].delta;
    uint64_t instructions = counters[1].delta;
    uint64_t load_misses_walk_duration= counters[2].delta;
    uint64_t store_misses_walk_duration = counters[3].delta;
    uint64_t load_misses_walk_completed = counters[4].delta;
    uint64_t store_misses_walk_completed = counters[5].delta;
    uint64_t ept_walk_cycles = counters[6].delta;

    uint64_t rss = get_rss_in_bytes(pid);
    double rss_in_gb = (double)rss / (1024 * 1024 * 1024); // Convert bytes to GB

    // double tlb_load_miss_ratio = dtlb_loads ? (double)dtlb_load_misses / dtlb_loads : 0.0;
    // double tlb_store_miss_ratio = dtlb_stores ? (double)dtlb_store_misses / dtlb_stores : 0.0;
    // double ept_walk_ratio = cycles ? (double)ept_walk_cycles / cycles : 0.0;


    // printf("ept_walk_cycles: %lu, dtlb_load_misses: %lu, dtlb_store_misses: %lu\n", ept_walk_cycles, dtlb_load_misses, dtlb_store_misses);
    // printf("cycles: %lu, instructions: %lu\n", cycles, instructions);
    // printf("dtlb_loads: %lu, dtlb_stores: %lu\n", dtlb_loads, dtlb_stores);

    //  printf("ept_walk_ratio: %lf, tlb_load_miss_ratio: %lf, tlb_store_miss_ratio: %lf\n", ept_walk_ratio, tlb_load_miss_ratio, tlb_store_miss_ratio);

    uint64_t total_walk_cycles = load_misses_walk_duration + store_misses_walk_duration + ept_walk_cycles;

    uint64_t walks_completed = load_misses_walk_completed + store_misses_walk_completed;
    double avg_total_walk_cycles = (double)total_walk_cycles / (walks_completed ? walks_completed : 1);
    double avg_ept_walk_cycles = (double)ept_walk_cycles / (walks_completed ? walks_completed : 1);
    printf("ept_walk_cycles: %lu \n", ept_walk_cycles);
    printf("avg_ept_walk_cycles: %lf\n", avg_ept_walk_cycles);
    printf("avg_total_walk_cycles: %lf\n", avg_total_walk_cycles);

    double error_margin = 0.05;
    double lower_bound = AVG_WALK_CYCLES * (1.0 - error_margin);
    double upper_bound = AVG_WALK_CYCLES * (1.0 + error_margin);

    bool in_range = avg_ept_walk_cycles > lower_bound; // 5% margin on lower bound

    if (rss_in_gb >= 1.0 && in_range) {
        return 1;  // enable TPT
    }


    // simple threshold based decision (following a simple heuristic)
    // if (rss_in_gb >= 1 && ept_walk_ratio > 0.5 && (tlb_load_miss_ratio > 0.5 || tlb_store_miss_ratio > 0.5)) {
    //     return 1; // enable TPT
    // }

    return 0; // disable TPT
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
        read(pipefd[0], &dummy, 1); // blocking 
        close(pipefd[0]);

        execvp(program, argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // parent process; child pid 
        close(pipefd[0]);

        init_counters(counters, pid);

        for (int i = 0; i < COUNTER_COUNT; i++) {
            if (ioctl(counters[i].fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
                fprintf(stderr, "Failed to enable %s: %s\n", counters[i].name, strerror(errno));
            }
        }

        write(pipefd[1], "G", 1);
        close(pipefd[1]);

        int status;
        time_t last_sample_time = time(NULL);
        bool enabled_tpt = false;

        // while the child process is running
        while (waitpid(pid, &status, WNOHANG) == 0) {
            time_t current_time = time(NULL);
            if (!enabled_tpt && (current_time - last_sample_time >= SAMPLING_INTERVAL_SEC)) {
                sample_counters(counters);

                if (should_enable_tpt(counters, pid)) {
                    printf("=========== ACTION : ENABLE TPT ===========\n");
                    enabled_tpt = true;
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
