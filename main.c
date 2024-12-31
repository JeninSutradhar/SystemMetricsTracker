#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <time.h>
#include <dirent.h>
#include <stdint.h>
#include <fcntl.h>
#include <ncurses.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <linux/limits.h>

#define PROC_STAT "/proc/stat"
#define PROC_MEMINFO "/proc/meminfo"
#define PROC_NET_DEV "/proc/net/dev"
#define LOG_FILE "system_metrics.log"
#define MAX_PROCESSES 10
#define MAX_SENSOR_LEN 64
#define MAX_VALUE_LEN 32

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct {
    unsigned long long int user;
    unsigned long long int nice;
    unsigned long long int system;
    unsigned long long int idle;
    unsigned long long int iowait;
    unsigned long long int irq;
    unsigned long long int softirq;
} CPUStats;

typedef struct {
    char name[64];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
} NetworkStats;

typedef struct {
    int pid;
    char name[256];
    double cpu_usage;
    unsigned long long memory_usage;
} ProcessInfo;

void log_error(const char *message) {
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp != NULL) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(log_fp, "[%d-%02d-%02d %02d:%02d:%02d] Error: %s\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, message);
        fclose(log_fp);
    }
}

int get_cpu_stats(CPUStats *stats) {
    FILE *fp = fopen(PROC_STAT, "r");
    if (fp == NULL) {
        perror("Error opening /proc/stat");
        log_error("Failed to open /proc/stat");
        return -1;
    }

    char buffer[512];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strncmp(buffer, "cpu ", 4) == 0) {
            sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu",
                   &stats->user, &stats->nice, &stats->system, &stats->idle,
                   &stats->iowait, &stats->irq, &stats->softirq);
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

void print_cpu_usage(WINDOW *win, CPUStats *prev, CPUStats *current) {
    if (get_cpu_stats(current) != 0) return;

    if (prev->user != 0 || prev->nice != 0 || prev->system != 0 || prev->idle != 0 || prev->iowait != 0 || prev->irq != 0 || prev->softirq != 0) {
        unsigned long long prev_total = prev->user + prev->nice + prev->system + prev->idle + prev->iowait + prev->irq + prev->softirq;
        unsigned long long current_total = current->user + current->nice + current->system + current->idle + current->iowait + current->irq + current->softirq;
        unsigned long long total_diff = current_total - prev_total;
        unsigned long long idle_diff = current->idle - prev->idle;

        if (total_diff > 0) {
            double cpu_usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
            mvwprintw(win, 2, 2, "CPU Usage: %.2f%%", cpu_usage);
            mvwprintw(win, 3, 2, "  User: %.2f%%, System: %.2f%%, Nice: %.2f%%",
                    100.0 * (current->user - prev->user) / total_diff,
                    100.0 * (current->system - prev->system) / total_diff,
                    100.0 * (current->nice - prev->nice) / total_diff);
            mvwprintw(win, 4, 2, "  Iowait: %.2f%%, Irq: %.2f%%, Softirq: %.2f%%",
                    100.0 * (current->iowait - prev->iowait) / total_diff,
                    100.0 * (current->irq - prev->irq) / total_diff,
                    100.0 * (current->softirq - prev->softirq) / total_diff);
        } else {
            mvwprintw(win, 2, 2, "CPU Usage: N/A");
        }
    } else {
        mvwprintw(win, 2, 2, "CPU Usage: ");
    }
    *prev = *current;
}

void print_memory_usage(WINDOW *win) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        perror("Error getting system info");
        log_error("Failed to get system info");
        return;
    }

    double total_ram = (double)info.totalram * info.mem_unit / (1024 * 1024 * 1024);
    double free_ram = (double)info.freeram * info.mem_unit / (1024 * 1024 * 1024);
    double used_ram = total_ram - free_ram;
    double usage_percent = (used_ram / total_ram) * 100;

    double total_swap = (double)info.totalswap * info.mem_unit / (1024 * 1024 * 1024);
    double free_swap = (double)info.freeswap * info.mem_unit / (1024 * 1024 * 1024);
    double used_swap = total_swap - free_swap;
    double swap_usage_percent = (used_swap / total_swap) * 100;

    mvwprintw(win, 2, 2, "Memory Usage: %.2f GB / %.2f GB (%.2f%%)", used_ram, total_ram, usage_percent);
    mvwprintw(win, 3, 2, "  Swap: %.2f GB / %.2f GB (%.2f%%)", used_swap, total_swap, swap_usage_percent);
}

int get_network_stats(NetworkStats *stats, int count) {
    FILE *fp = fopen(PROC_NET_DEV, "r");
    if (fp == NULL) {
        perror("Error opening /proc/net/dev");
        log_error("Failed to open /proc/net/dev");
        return -1;
    }

    char buffer[512];
    int i = 0;
    // Skip header lines
    fgets(buffer, sizeof(buffer), fp);
    fgets(buffer, sizeof(buffer), fp);

    while (fgets(buffer, sizeof(buffer), fp) != NULL && i < count) {
        unsigned long long rx_packets, tx_packets, rx_errs, tx_errs, rx_drop, tx_drop, rx_fifo, tx_fifo, rx_frame, tx_colls;
        if (sscanf(buffer, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   stats[i].name, &stats[i].rx_bytes, &rx_packets, &rx_errs, &rx_drop, &rx_fifo, &rx_frame,
                   &stats[i].tx_bytes, &tx_packets, &tx_errs, &tx_colls) == 11) {
            // Remove trailing ':' from interface name
            stats[i].name[strcspn(stats[i].name, ":")] = 0;
            i++;
        }
    }
    fclose(fp);
    return i;
}

void print_network_usage(WINDOW *win, NetworkStats *prev_stats, NetworkStats *current_stats, int count, double elapsed_time) {
    int current_count = get_network_stats(current_stats, count);
    if (current_count <= 0) return;

    mvwprintw(win, 2, 2, "Network Usage:");
    double total_down_speed = 0.0;
    double total_up_speed = 0.0;
    unsigned long long total_down_bytes = 0;
    unsigned long long total_up_bytes = 0;

    for (int i = 0; i < current_count; i++) {
        for (int j = 0; j < count; j++) {
            if (strcmp(current_stats[i].name, prev_stats[j].name) == 0) {
                unsigned long long rx_diff = current_stats[i].rx_bytes - prev_stats[j].rx_bytes;
                unsigned long long tx_diff = current_stats[i].tx_bytes - prev_stats[j].tx_bytes;
                double down_speed = (double)rx_diff / elapsed_time / 1024.0; // KB/s
                double up_speed = (double)tx_diff / elapsed_time / 1024.0;     // KB/s
                total_down_speed += down_speed;
                total_up_speed += up_speed;
                total_down_bytes += current_stats[i].rx_bytes;
                total_up_bytes += current_stats[i].tx_bytes;
                break;
            }
        }
        mvwprintw(win, 3 + i, 4, "%s: Down: %.2f KB/s, Up: %.2f KB/s", current_stats[i].name, 0.0, 0.0); // Placeholder for per-interface details
    }
    mvwprintw(win, 3, 2, "Total Down: %.2f KB/s, Total Up: %.2f KB/s", total_down_speed, total_up_speed);
    mvwprintw(win, 4, 2, "Total Downloaded: %.2f MB, Total Uploaded: %.2f MB", (double)total_down_bytes / 1024.0 / 1024.0, (double)total_up_bytes / 1024.0 / 1024.0);

    memcpy(prev_stats, current_stats, current_count * sizeof(NetworkStats));
}

int compare_processes_cpu(const void *a, const void *b) {
    return ((ProcessInfo *)b)->cpu_usage - ((ProcessInfo *)a)->cpu_usage;
}

int compare_processes_mem(const void *a, const void *b) {
    return ((ProcessInfo *)b)->memory_usage - ((ProcessInfo *)a)->memory_usage;
}

int get_top_processes(ProcessInfo *processes, int max_processes, int sort_by_cpu) {
    DIR *dir;
    struct dirent *ent;
    int process_count = 0;

    dir = opendir("/proc");
    if (dir != NULL) {
        while ((ent = readdir(dir)) != NULL && process_count < max_processes) {
            int pid = atoi(ent->d_name);
            if (pid > 0) {
                char stat_path[256];
                sprintf(stat_path, "/proc/%d/stat", pid);
                FILE *stat_fp = fopen(stat_path, "r");
                if (stat_fp != NULL) {
                    char buffer[512];
                    fgets(buffer, sizeof(buffer), stat_fp);
                    unsigned long utime, stime;
                    char comm[256];
                    sscanf(buffer, "%d %s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu",
                           &processes[process_count].pid, comm, &utime, &stime);
                    // Remove parentheses from process name
                    sscanf(comm, "(%[^)])", processes[process_count].name);
                    fclose(stat_fp);

                    char status_path[256];
                    sprintf(status_path, "/proc/%d}/status", pid);
                    FILE *status_fp = fopen(status_path, "r");
                    if (status_fp != NULL) {
                        while (fgets(buffer, sizeof(buffer), status_fp)) {
                            if (strncmp(buffer, "VmRSS:", 6) == 0) {
                                sscanf(buffer, "VmRSS: %llu", &processes[process_count].memory_usage);
                                processes[process_count].memory_usage *= 1024; // Convert KB to bytes
                                break;
                            }
                        }
                        fclose(status_fp);
                    }

                    // Placeholder for CPU usage calculation (requires previous values)
                    processes[process_count].cpu_usage = 0.0;
                    process_count++;
                }
            }
        }
        closedir(dir);
    } else {
        perror("Could not open /proc");
        log_error("Failed to open /proc");
    }

    if (sort_by_cpu) {
        qsort(processes, process_count, sizeof(ProcessInfo), compare_processes_cpu);
    } else {
        qsort(processes, process_count, sizeof(ProcessInfo), compare_processes_mem);
    }

    return process_count;
}

void print_top_processes(WINDOW *win, int sort_by_cpu) {
    ProcessInfo processes[MAX_PROCESSES];
    int process_count = get_top_processes(processes, MAX_PROCESSES, sort_by_cpu);

    mvwprintw(win, 2, 2, "Top Processes (by %s):", sort_by_cpu ? "CPU" : "Memory");
    for (int i = 0; i < process_count; i++) {
        mvwprintw(win, 3 + i, 4, "%d: %s - CPU: %.2f%%, Mem: %.2f MB",
                  processes[i].pid, processes[i].name, processes[i].cpu_usage, (double)processes[i].memory_usage / 1024.0 / 1024.0);
    }
}

int read_sensor_value(const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return INT_MIN; // Indicate error
    }
    int value;
    if (fscanf(fp, "%d", &value) != 1) {
        fclose(fp);
        return INT_MIN;
    }
    fclose(fp);
    return value;
}

char *strreplace(char *orig, const char *rep, const char *with) {
    char *result;
    const char *ins = orig;
    char *tmp;
    int len_rep = strlen(rep);
    int len_with = strlen(with);
    int count;

    for (count = 0; (tmp = strstr(ins, rep)) != NULL; ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        int part_len = ins - orig;
        memcpy(tmp, orig, part_len);
        tmp += part_len;
        memcpy(tmp, with, len_with);
        tmp += len_with;
        orig += part_len + len_rep;
    }

    strcpy(tmp, orig);
    return result;
}

void print_temperature_fan_speeds(WINDOW *win) {
    mvwprintw(win, 2, 2, "Temperature & Fan Speeds:");
    int line = 3;

    DIR *dir_hwmon = opendir("/sys/class/hwmon");
    if (dir_hwmon != NULL) {
        struct dirent *ent_hwmon;
        while ((ent_hwmon = readdir(dir_hwmon)) != NULL) {
            if (strncmp(ent_hwmon->d_name, "hwmon", 5) == 0) {
                char hwmon_path[PATH_MAX];
                snprintf(hwmon_path, sizeof(hwmon_path), "/sys/class/hwmon/%s", ent_hwmon->d_name);

                // Read temperatures
                DIR *dir_temp = opendir(hwmon_path);
                if (dir_temp != NULL) {
                    struct dirent *ent_temp;
                    while ((ent_temp = readdir(dir_temp)) != NULL) {
                        if (strncmp(ent_temp->d_name, "temp", 4) == 0 && strstr(ent_temp->d_name, "_input")) {
                            char temp_path[PATH_MAX];
                            snprintf(temp_path, sizeof(temp_path), "%s/%s", hwmon_path, ent_temp->d_name);
                            int temp = read_sensor_value(temp_path);
                            if (temp != INT_MIN) {
                                // Try to read the corresponding label
                                char label_path[PATH_MAX];
                                snprintf(label_path, sizeof(label_path), "%s/%s", hwmon_path, strreplace(strdup(ent_temp->d_name), "input", "label"));
                                FILE *fp_label = fopen(label_path, "r");
                                char label[MAX_SENSOR_LEN];
                                if (fp_label && fgets(label, sizeof(label), fp_label)) {
                                    label[strcspn(label, "\n")] = 0; // Remove newline
                                    mvwprintw(win, line++, 4, "Temp (%s): %.1f°C", label, temp / 1000.0);
                                    fclose(fp_label);
                                } else {
                                    mvwprintw(win, line++, 4, "Temperature: %.1f°C", temp / 1000.0);
                                }
                            }
                        } else if (strncmp(ent_temp->d_name, "fan", 3) == 0 && strstr(ent_temp->d_name, "_input")) {
                            char fan_path[PATH_MAX];
                            snprintf(fan_path, sizeof(fan_path), "%s/%s", hwmon_path, ent_temp->d_name);
                            int fan_speed = read_sensor_value(fan_path);
                            if (fan_speed != INT_MIN) {
                                char label_path[PATH_MAX];
                                snprintf(label_path, sizeof(label_path), "%s/%s", hwmon_path, strreplace(strdup(ent_temp->d_name), "input", "label"));
                                FILE *fp_label = fopen(label_path, "r");
                                char label[MAX_SENSOR_LEN];
                                if (fp_label && fgets(label, sizeof(label), fp_label)) {
                                    label[strcspn(label, "\n")] = 0;
                                    mvwprintw(win, line++, 4, "Fan (%s): %d RPM", label, fan_speed);
                                    fclose(fp_label);
                                } else {
                                    mvwprintw(win, line++, 4, "Fan Speed: %d RPM", fan_speed);
                                }
                            }
                        }
                    }
                    closedir(dir_temp);
                }
            }
        }
        closedir(dir_hwmon);
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    scrollok(stdscr, TRUE);
    curs_set(0);

    int height, width;
    getmaxyx(stdscr, height, width);

    WINDOW *cpu_win = newwin(7, width - 2, 1, 1);
    box(cpu_win, 0, 0);
    mvwprintw(cpu_win, 0, 1, "CPU Usage");

    WINDOW *mem_win = newwin(5, width - 2, 8, 1);
    box(mem_win, 0, 0);
    mvwprintw(mem_win, 0, 1, "Memory Usage");

    WINDOW *net_win = newwin(7, width - 2, 13, 1);
    box(net_win, 0, 0);
    mvwprintw(net_win, 0, 1, "Network Usage");

    WINDOW *proc_win = newwin(12, width - 2, 20, 1);
    box(proc_win, 0, 0);
    mvwprintw(proc_win, 0, 1, "Top Processes");

    WINDOW *sensor_win = newwin(10, width - 2, 32, 1);
    box(sensor_win, 0, 0);
    mvwprintw(sensor_win, 0, 1, "Sensors");

    CPUStats prev_cpu_stats = {0}, current_cpu_stats = {0};
    NetworkStats prev_net_stats[10] = {0}; // Assume max 10 interfaces
    NetworkStats current_net_stats[10] = {0};
    int net_interface_count = 10; // Maximum number of interfaces to track

    time_t prev_time = time(NULL);

    while (1) {
        // No need to erase the whole screen here
        getmaxyx(stdscr, height, width); // Update dimensions if terminal resized

        // Update window sizes and positions if terminal resized
        wresize(cpu_win, 7, width - 2);
        wresize(mem_win, 5, width - 2);
        mvwin(mem_win, 8, 1);
        wresize(net_win, 7, width - 2);
        mvwin(net_win, 13, 1);
        wresize(proc_win, 12, width - 2);
        mvwin(proc_win, 20, 1);
        wresize(sensor_win, 10, width - 2);
        mvwin(sensor_win, 32, 1);

        box(cpu_win, 0, 0);
        mvwprintw(cpu_win, 0, 1, "CPU Usage");
        print_cpu_usage(cpu_win, &prev_cpu_stats, &current_cpu_stats);
        wrefresh(cpu_win);

        box(mem_win, 0, 0);
        mvwprintw(mem_win, 0, 1, "Memory Usage");
        print_memory_usage(mem_win);
        wrefresh(mem_win);

        box(net_win, 0, 0);
        mvwprintw(net_win, 0, 1, "Network Usage");
        time_t current_time = time(NULL);
        double elapsed_time = difftime(current_time, prev_time);
        print_network_usage(net_win, prev_net_stats, current_net_stats, net_interface_count, elapsed_time);
        wrefresh(net_win);
        prev_time = current_time;

        box(proc_win, 0, 0);
        mvwprintw(proc_win, 0, 1, "Top Processes (CPU)");
        print_top_processes(proc_win, 1);
        wrefresh(proc_win);

        box(sensor_win, 0, 0);
        mvwprintw(sensor_win, 0, 1, "Sensors");
        print_temperature_fan_speeds(sensor_win);
        wrefresh(sensor_win);

        refresh(); // Refresh the standard screen only ONCE at the end

        napms(3000); // Adjust the sleep time as needed
    }

    endwin();
    return 0;
}