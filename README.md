# System Metrics Tracker

A command-line tool to monitor system performance metrics in real-time within your terminal.

## Features

* **Real-time System Monitoring** 
* **CPU Usage** 
* **Memory Usage** 
* **Disk Usage** 
* **Network Usage**
    * Monitors network bandwidth
    * Displays total data transferred
* **Top Processes** 
* **Temperature and Fan Speeds**
* 
## Getting Started
### Prerequisites
ensure you have the following installed on your system:

* **A Linux-based Operating System:** This tool is designed specifically for Linux.
* **GCC (GNU Compiler Collection):**  You'll need a C compiler to build the application.
* **ncurses Library:** The `ncurses` library is required for the terminal UI.

**Installation of Prerequisites (Example for Debian/Ubuntu):**

```bash
sudo apt update
sudo apt install build-essential libncurses-dev
```

**Installation of Prerequisites (Example for Fedora/CentOS/RHEL):**

```bash
sudo dnf install gcc ncurses-devel
```

# Installation

1. **Clone the Repository:**

   ```bash
   git clone https://github.com/JeninSutradhar/system-metrics-tracker.git
   cd system-metrics-tracker
   ```
   **(Replace `your-username/system-metrics-tracker` with the actual repository URL)**

2. **Compile the Code:**

   ```bash
   gcc system_metrics.c -o system_metrics -lncurses
   ```

   * `-lncurses`: Links the `ncurses` library.

5. **Run the Executable:**

   ```bash
   ./system_metrics
   ```


### System Compatibility

This tool is primarily compatible with **Linux-based operating systems**. It relies on the `/proc` and `/sys` virtual file systems, which are core components of the Linux kernel.

* **Tested Distributions:** This tool has been tested on common Linux distributions.
* **Kernel Requirements:**  Requires a Linux kernel that exposes system information through the `/proc` and `/sys` file systems.
* **Hardware Monitoring:** The temperature and fan speed monitoring functionality depends on the presence of hardware monitoring interfaces exposed through the `/sys/class/hwmon` directory. The availability and specific naming conventions of these sensors might vary depending on your hardware and kernel drivers.

## Contributing

Contributions to the System Metrics Tracker are welcome! If you have ideas for improvements, bug fixes, or new features, feel free to:

## License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for more details.

