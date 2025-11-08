// sysmonitor.cpp
// Single-file System Monitor (assistant's code updated per your requests)
// Compile:
//   g++ -std=c++17 -O2 -Wall sysmonitor.cpp -lncurses -o sysmonitor
// Run:
//   ./sysmonitor

#include <bits/stdc++.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <signal.h>
#include <ncurses.h>

using namespace std;

struct Proc {
    int pid = 0;
    string cmd;
    string state;
    unsigned long long jiffies = 0; // utime + stime
    double cpu = 0.0;  // percentage
    double mem = 0.0;  // percentage
};

static bool is_digits(const string &s){
    if(s.empty()) return false;
    for(char c: s) if(!isdigit((unsigned char)c)) return false;
    return true;
}

// read /proc/stat first line -> fill total and idle jiffies
static void read_total_and_idle(unsigned long long &total, unsigned long long &idle) {
    total = 0; idle = 0;
    ifstream f("/proc/stat");
    string line;
    if (!getline(f, line)) return;
    stringstream ss(line);
    string cpu;
    unsigned long long user=0, nice=0, system=0, idle_t=0, iowait=0, irq=0, softirq=0, steal=0;
    ss >> cpu >> user >> nice >> system >> idle_t >> iowait >> irq >> softirq >> steal;
    idle = idle_t + iowait;
    total = user + nice + system + idle_t + iowait + irq + softirq + steal;
}

// read total memory (KB)
static unsigned long read_mem_total_kb() {
    ifstream f("/proc/meminfo");
    string line;
    while (getline(f, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            stringstream ss(line);
            string key; unsigned long val;
            ss >> key >> val;
            return val; // KB
        }
    }
    return 0;
}

// read basic proc info: comm, state, utime+stime (jiffies) and VmRSS (KB)
static bool read_proc_basic(int pid, unsigned long long &pjiffies, long &vmrss_kb, string &comm, string &state) {
    pjiffies = 0; vmrss_kb = 0; comm.clear(); state = "?";
    string statPath = "/proc/" + to_string(pid) + "/stat";
    ifstream sf(statPath);
    if (!sf.is_open()) return false;
    string statLine;
    getline(sf, statLine);
    // comm inside parentheses; find them
    size_t lp = statLine.find('(');
    size_t rp = statLine.rfind(')');
    if (lp == string::npos || rp == string::npos || rp <= lp) return false;
    comm = statLine.substr(lp+1, rp-lp-1);
    // make a stream from remainder after ") "
    string rest = statLine.substr(rp + 2);
    stringstream ss(rest);
    ss >> state; // state is first token after comm
    // We need utime (field 14) and stime (15) relative to full stat; since we cut rest, we must parse tokens.
    // In rest, fields start at position 3. utime becomes index 11 (0-based) and stime 12.
    vector<unsigned long long> vals;
    unsigned long long v;
    while (ss >> v) vals.push_back(v);
    if (vals.size() >= 13) {
        unsigned long long utime = vals[11];
        unsigned long long stime = vals[12];
        pjiffies = utime + stime;
    } else {
        pjiffies = 0;
    }
    // VmRSS from status
    string statusPath = "/proc/" + to_string(pid) + "/status";
    ifstream stf(statusPath);
    string line;
    while (getline(stf, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            stringstream ms(line);
            string key; long val;
            ms >> key >> val;
            vmrss_kb = val; // KB
            break;
        }
    }
    return true;
}

int main() {
    // ncurses init
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    // mouse support (optional)
    mousemask(ALL_MOUSE_EVENTS, NULL);
    mouseinterval(0);

    long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus <= 0) num_cpus = 1;

    // previous totals for deltas
    unsigned long long prevTotal = 0, prevIdle = 0;
    read_total_and_idle(prevTotal, prevIdle);

    unordered_map<int, unsigned long long> prevProcJ;
    unsigned long memTotalKB = read_mem_total_kb();

    // sort_mode: 1=CPU default, 2=MEM, 0=PID
    int sort_mode = 1;

    int selectedIndex = 0;
    int selectedPid = -1;

    // main loop
    while (true) {
        // get totals
        unsigned long long totalNow=0, idleNow=0;
        read_total_and_idle(totalNow, idleNow);
        unsigned long long totalDiff = (totalNow > prevTotal) ? totalNow - prevTotal : 1;
        // refresh memTotal occasionally
        memTotalKB = read_mem_total_kb();

        // gather process list
        vector<Proc> procs;
        DIR *d = opendir("/proc");
        if (!d) { mvprintw(0,0,"Failed to open /proc"); refresh(); endwin(); return 1; }
        struct dirent *entry;
        while ((entry = readdir(d)) != nullptr) {
            string dname = entry->d_name;
            if (!is_digits(dname)) continue;
            int pid = stoi(dname);
            unsigned long long pj = 0;
            long vmrss = 0;
            string comm, state;
            if (!read_proc_basic(pid, pj, vmrss, comm, state)) continue;
            Proc p;
            p.pid = pid;
            p.cmd = comm;
            p.state = state;
            p.jiffies = pj;
            // compute delta vs previous
            unsigned long long prevPJ = 0;
            auto it = prevProcJ.find(pid);
            if (it != prevProcJ.end()) prevPJ = it->second;
            unsigned long long deltaP = (pj > prevPJ) ? (pj - prevPJ) : 0ULL;
            // per-process CPU% = deltaP / totalDiff * 100 * num_cpus
            double cpuPct = 0.0;
            if (totalDiff > 0) cpuPct = (double)deltaP / (double)totalDiff * 100.0 * (double)num_cpus;
            p.cpu = cpuPct;
            double memPct = 0.0;
            if (memTotalKB > 0) memPct = (double)vmrss / (double)memTotalKB * 100.0;
            p.mem = memPct;
            procs.push_back(p);
            prevProcJ[pid] = pj;
        }
        closedir(d);

        // sort
        if (sort_mode == 1) {
            sort(procs.begin(), procs.end(), [](const Proc &a, const Proc &b){
                return a.cpu > b.cpu;
            });
        } else if (sort_mode == 2) {
            sort(procs.begin(), procs.end(), [](const Proc &a, const Proc &b){
                return a.mem > b.mem;
            });
        } else {
            sort(procs.begin(), procs.end(), [](const Proc &a, const Proc &b){
                return a.pid < b.pid;
            });
        }

        // preserve selection by PID
        if (selectedPid != -1) {
            bool found = false;
            for (size_t i = 0; i < procs.size(); ++i) {
                if (procs[i].pid == selectedPid) { selectedIndex = (int)i; found = true; break; }
            }
            if (!found) {
                if (selectedIndex >= (int)procs.size()) selectedIndex = max(0, (int)procs.size() - 1);
            }
        } else {
            if (selectedIndex >= (int)procs.size()) selectedIndex = max(0, (int)procs.size() - 1);
        }

        // draw UI (plain, no color)
        clear();
        attron(A_BOLD);
        mvprintw(0, 2, "=== SYSTEM MONITOR ===");
        attroff(A_BOLD);

        // uptime + process count
        struct sysinfo s;
        sysinfo(&s);
        int hours = s.uptime / 3600;
        int mins = (s.uptime % 3600) / 60;
        mvprintw(1, 2, "System Monitor - Uptime: %dh %dm | Processes: %zu", hours, mins, procs.size());

        // memory summary
        long totalMB = (s.totalram / 1024) / 1024;
        long usedMB = ((s.totalram - s.freeram) / 1024) / 1024;
        long freeMB = (s.freeram / 1024) / 1024;
        mvprintw(2, 2, "Memory: Total: %ld MB | Used: %ld MB | Free: %ld MB", totalMB, usedMB, freeMB);

        // overall CPU usage (using idle & total)
        unsigned long long idleDiff = (idleNow > prevIdle) ? idleNow - prevIdle : 0ULL;
        unsigned long long totalDiffForOverall = (totalNow > prevTotal) ? totalNow - prevTotal : 1ULL;
        double overallCpu = (double)(totalDiffForOverall - idleDiff) / (double)totalDiffForOverall * 100.0;
        mvprintw(3, 2, "CPU Usage: %.2f%%", overallCpu);

        // controls
        mvprintw(4, 2, "Controls: [c]CPU [m]Memory [p]PID | [k]Kill | [q]Quit | Arrow keys: Navigate | Mouse: click to select");

        // header for list (STATE column removed; tightened spacing)
        int start_row = 6;
        // PROCESS column wider and CPU/MEM printed immediately after
        mvprintw(start_row-1, 2, "%-8s %-40s %-7s %-7s", "PID", "PROCESS", "CPU%", "MEM%");

        int maxrows = LINES - start_row - 3;
        if (maxrows < 5) maxrows = 5;
        int display = min((int)procs.size(), maxrows);

        for (int i = 0; i < display; ++i) {
            int row = start_row + i;
            if (i == selectedIndex) attron(A_REVERSE);
            // trim command
            string cmd = procs[i].cmd;
            if ((int)cmd.size() > 40) cmd = cmd.substr(0, 37) + "...";
            mvprintw(row, 2, "%-8d %-40s %6.2f %6.2f",
                     procs[i].pid,
                     cmd.c_str(),
                     procs[i].cpu,
                     procs[i].mem);
            if (i == selectedIndex) attroff(A_REVERSE);
        }

        // sort mode text at bottom
        const char *modeText = (sort_mode==0 ? "PID" : (sort_mode==1 ? "CPU" : "Memory"));
        mvprintw(LINES-1, 2, "Sort Mode: %s", modeText);

        refresh();

        // input
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;
        else if (ch == 'c' || ch == 'C') { sort_mode = 1; selectedIndex = 0; selectedPid = -1; }
        else if (ch == 'm' || ch == 'M') { sort_mode = 2; selectedIndex = 0; selectedPid = -1; }
        else if (ch == 'p' || ch == 'P') { sort_mode = 0; selectedIndex = 0; selectedPid = -1; }
        else if (ch == KEY_UP) {
            if (selectedIndex > 0) { selectedIndex--; if (selectedIndex < (int)procs.size()) selectedPid = procs[selectedIndex].pid; }
        }
        else if (ch == KEY_DOWN) {
            if (selectedIndex + 1 < (int)procs.size()) { selectedIndex++; if (selectedIndex < (int)procs.size()) selectedPid = procs[selectedIndex].pid; }
        }
        else if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK) {
                if (ev.bstate & BUTTON1_CLICKED) {
                    int clicked = ev.y - start_row;
                    if (clicked >= 0 && clicked < display) {
                        selectedIndex = clicked;
                        if (selectedIndex < (int)procs.size()) selectedPid = procs[selectedIndex].pid;
                    }
                }
            }
        }
        else if (ch == 'k' || ch == 'K') {
            if (selectedIndex >= 0 && selectedIndex < (int)procs.size()) {
                int pid = procs[selectedIndex].pid;
                if (kill(pid, SIGTERM) == 0) {
                    mvprintw(LINES-2, 2, "Sent SIGTERM to PID %d", pid);
                } else {
                    mvprintw(LINES-2, 2, "Failed to kill PID %d (permission?)", pid);
                }
                refresh();
                napms(600);
            }
        }

        // prepare for next iteration deltas
        prevTotal = totalNow;
        prevIdle = idleNow;

        napms(300); // refresh rate
    }

    endwin();
    return 0;
}

