#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024
#define MAX_ENTRIES 1

// define constants for thresholds 
#define EXEC_TIME_THRESHOLD 2.0   // 10 seconds
#define RSS_THRESHOLD_GB 4.0     // 4 GB of RSS

#define FREQUENCY 2.5e9 // 2.5 GHz 

typedef struct {
    double rss; // this here is the buffer size
    double cycles;
    double instructions;
    double dtlb_load_misses;
    double dtlb_loads;
    double dtlb_store_misses;
    double dtlb_stores;
    double miss_causes_a_walk_load;
    double stlb_hit_load;
    double walk_completed_load;
    double walk_duration_load;
    double miss_causes_a_walk_store;
    double stlb_hit_store;
    double walk_completed_store;
    double walk_duration_store;
    double ept_walk_cycles;
    double dtlb_l1_load;
    double dtlb_l2_load;
    double dtlb_l3_load;
    double dtlb_memory_load;
    double page_faults;
    double major_faults;
    double minor_faults;
} perf_data_t;

int read_csv_data(const char *filename, perf_data_t **data) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return -1;
    }

    char line[MAX_LINE_LENGTH]; // 1 exluding the header
    int index = 0;

    // Skip the header
    fgets(line, sizeof(line), file);

    *data = malloc(sizeof(perf_data_t));
    if (*data == NULL) {
        perror("Failed to allocate memory");
        fclose(file);
        return -1;
    }

    if (fgets(line, sizeof(line), file)) {
        perf_data_t *entry = *data;

        // Temporary buffer for the description string
        char description[256];

        int result = sscanf(line, "\"%[^\"]\",%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
            description,
            &entry->cycles,
            &entry->instructions,
            &entry->dtlb_load_misses,
            &entry->dtlb_loads,
            &entry->dtlb_store_misses,
            &entry->dtlb_stores,
            &entry->miss_causes_a_walk_load,
            &entry->stlb_hit_load,
            &entry->walk_completed_load,
            &entry->walk_duration_load,
            &entry->miss_causes_a_walk_store,
            &entry->stlb_hit_store,
            &entry->walk_completed_store,
            &entry->walk_duration_store,
            &entry->ept_walk_cycles,
            &entry->dtlb_l1_load,
            &entry->dtlb_l2_load,
            &entry->dtlb_l3_load,
            &entry->dtlb_memory_load,
            &entry->page_faults,
            &entry->major_faults,
            &entry->minor_faults
        );      
        

        printf("Parsed: %s\n", description);  // Todo: Only for testing
        // populate the rss field of the struct - this is the buffer size in this case 
        // in the external predictor logic it would be read from the /proc/self/status file
        // Extract block size from description
        entry->rss = 0; // Initialize rss to 0
        char *block_str = strstr(description, "block-size=");
        if (block_str) {
            block_str += strlen("block-size=");  // move past "block-size="
            char size_unit;
            double size_value;
            if (sscanf(block_str, "%lf%c", &size_value, &size_unit) == 2) {
                if (size_unit == 'G' || size_unit == 'g') {
                    entry->rss = size_value;
                } 
            } 
        }
    }

    fclose(file);
    return 1;
}

double compute_execution_time(perf_data_t *entry) {
    return entry->cycles / FREQUENCY; // this is in seconds 
}

// this function currently is a simple threshold based decision function
// if this function returns true(1) then we enable TPT which is used for the further runs of the program
int should_enable_tpt(perf_data_t *entry) {
    double execution_time = compute_execution_time(entry); // execution time in seconds
    printf("Execution time: %lf seconds\n", execution_time);
    double rss_in_gb = entry->rss; // assume rss stored in GB 
    // primary decision based on RSS value -> less memory usage - not an memory intensive workload
    // if (rss_in_gb < RSS_THRESHOLD_GB) {
    //     return 0; // disable TPT
    // }
    // primary decision based on execution time
    if (execution_time < EXEC_TIME_THRESHOLD) {
        return 0; // disable TPT
    }

    // check if most of the cycles is spent on address translation 
    double dtlb_load_misses_ratio = entry->dtlb_load_misses / entry->dtlb_loads;
    double dtlb_store_misses_ratio = entry->dtlb_store_misses / entry->dtlb_stores;
    double ept_walk_cycles_ratio = entry->ept_walk_cycles / entry->cycles; // crutial -> majority of cycles 

    printf("DTLB Load Misses Ratio: %lf\n", dtlb_load_misses_ratio);
    printf("DTLB Store Misses Ratio: %lf\n", dtlb_store_misses_ratio);
    printf("EPT Walk Cycles Ratio: %lf\n", ept_walk_cycles_ratio);

    if (ept_walk_cycles_ratio > 0.5 && (dtlb_load_misses_ratio > 0.5 || dtlb_store_misses_ratio > 0.5)) {
        return 1; // enable TPT
    }
    return 0; // disable TPT

}

int main(int argc, char *argv[]) {
    char *filename = "test_data.csv";
    perf_data_t *data = NULL;
    int num_entries = read_csv_data(filename, &data);
    if (num_entries < 0) {
        return EXIT_FAILURE;
    }

    // make decision 

    if (should_enable_tpt(data)) {
        printf("TPT enabled\n");
    } else {
        printf("TPT disabled\n");
    }

    free(data);
    return EXIT_SUCCESS;
}
