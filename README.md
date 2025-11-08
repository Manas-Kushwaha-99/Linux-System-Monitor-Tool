## 🖥️ System Monitor (C++ & Ncurses)

A lightweight, terminal-based **System Monitoring Tool** written in **C++** using the **Ncurses** library.
It displays live CPU and memory usage, running processes, and lets you terminate tasks directly from the terminal.

---

### ⚙️ Features

* Real-time **CPU & Memory** monitoring
* Lists **PID, Process Name, CPU%, MEM%**
* Sortable by **CPU**, **Memory**, or **PID**
* **Keyboard navigation** and **mouse selection** support
* **Kill processes** safely using the terminal
* Displays **uptime** and **total running processes**
* Clean, fast, **no-color** interface
* Works natively on **Linux**

---

### 🧩 Requirements (Linux)

Make sure you have the following packages installed:

```bash
sudo apt update
sudo apt install g++ make libncurses5-dev libncursesw5-dev
```

---

### 🚀 Build & Run

```bash
# Compile
g++ -std=c++17 -O2 -Wall sysmonitor.cpp -lncurses -o sysmonitor

# Run
./sysmonitor
```

---

### 🎮 Controls

| Key   | Action                          |
| ----- | ------------------------------- |
| ↑ / ↓ | Move selection                  |
| Mouse | Click to select a process       |
| C     | Sort by CPU usage               |
| M     | Sort by Memory usage            |
| P     | Sort by PID                     |
| K     | Kill selected process (SIGTERM) |
| Q     | Quit the program                |

---

### 🧠 Technical Overview

* Uses **/proc** filesystem to fetch process and system statistics
* Reads CPU info from `/proc/stat`
* Reads Memory info from `/proc/meminfo`
* Reads per-process details (CPU time, memory usage) from `/proc/[pid]/stat` and `/proc/[pid]/status`
* Renders the interface using **Ncurses**, enabling keyboard and mouse interactivity

---

### 📸 Screenshots

<img width="868" height="538" alt="Screenshot From 2025-11-08 21-14-03" src="https://github.com/user-attachments/assets/df11670f-1906-4965-9d99-87bd3e258eac" />

---

### 🧾 Notes

* This tool must be run on a **Linux environment**, as it relies on the `/proc` filesystem.
* It does not require any additional libraries beyond **Ncurses**.
* Works in any terminal emulator that supports Ncurses (like GNOME Terminal, Konsole, or Alacritty).
