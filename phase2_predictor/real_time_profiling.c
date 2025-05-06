// this is a real time profiling tool - that collects metrics during the execution of the program
// and makes a decision to enable TPT (one way) for the rest of the execution
// this is a sampling based profiling tool extention for perf

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <time.h>

#define SAMPLE_INTERVAL 100000  // 100 ms
#define IPC_THRESHOLD 0.5       // Instruction per cycle threshold
#define MAX_SAMPLES 1000

typedef struct {
    uint64_t cycles;
    uint64_t instructions;
    long rss_kb;
} Sample;

Sample samples[MAX_SAMPLES];
int sample_index = 0;

int perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                    int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int open_perf_counter(uint64_t config, pid_t pid) {
    struct perf_event_attr pe = {
        .type = PERF_TYPE_HARDWARE,
        .size = sizeof(struct perf_event_attr),
        .config = config,
        .disabled = 0,
        .exclude_kernel = 0,
        .exclude_hv = 0,
    };
    return perf_event_open(&pe, pid, -1, -1, 0);
}

long get_rss_kb(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[256];
    long rss = -1;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss);
            break;
        }
    }
    fclose(fp);
    return rss;
}


void enable_tpt() {
    // Placeholder: implement the actual TPT enabling mechanism here
    printf("=== [ACTION] Required to enable TPT ===\n");
}

void collect_sample(pid_t pid, int fd_cycles, int fd_instr) {
    Sample s;

    ioctl(fd_cycles, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd_instr, PERF_EVENT_IOC_RESET, 0);

    usleep(SAMPLE_INTERVAL);

    read(fd_cycles, &s.cycles, sizeof(uint64_t));
    read(fd_instr, &s.instructions, sizeof(uint64_t));

    s.rss_kb = get_rss_kb(pid);

    samples[sample_index % MAX_SAMPLES] = s;

    printf("Sample %d: cycles=%lu, instr=%lu, rss=%ld KB",
        sample_index, s.cycles, s.instructions, s.rss_kb);

    // Decision logic for enabling TPT
    if (s.cycles > 0) {
        double ipc = (double) s.instructions / s.cycles;
        if (ipc < IPC_THRESHOLD) {
            enable_tpt();
        }
    }
    sample_index++;
}

void profile_child(pid_t pid) {
    int fd_cycles = open_perf_counter(PERF_COUNT_HW_CPU_CYCLES, pid);
    int fd_instr  = open_perf_counter(PERF_COUNT_HW_INSTRUCTIONS, pid);

    if (fd_cycles < 0 || fd_instr < 0) {
        perror("perf_event_open");
        exit(1);
    }

    collect_sample(pid, fd_cycles, fd_instr);
    

    close(fd_cycles);
    close(fd_instr);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <target_executable> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        execvp(argv[1], &argv[1]);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        printf("Started child process PID: %d\n", pid);
        profile_child(pid);
        int status;
        waitpid(pid, &status, 0);
        printf("Child exited with status %d\n", status);
    } else {
        perror("fork failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
