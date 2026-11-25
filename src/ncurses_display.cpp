
#include <curses.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "format.h"
#include "ncurses_display.h"
#include "system.h"

using std::string;
using std::to_string;

// Flag used to request a clean shutdown.
namespace
{
    volatile std::sig_atomic_t stop_requested = 0;

    void HandleStopSignal(int)
    {
        stop_requested = 1;
    }
}

// Displays CPU and memory utilization using a 50-character progress bar.
std::string NCursesDisplay::ProgressBar(float percent)
{
    const int size = 50;

    if (!std::isfinite(percent))
    {
        percent = 0.0f;
    }

    percent = std::clamp(percent, 0.0f, 1.0f);

    int bars = static_cast<int>(std::round(percent * size));

    std::string result{"["};

    for (int i = 0; i < size; ++i)
    {
        result += (i < bars) ? '|' : ' ';
    }

    result += "] ";

    std::ostringstream display;
    display << std::fixed << std::setprecision(1)
            << percent * 100 << "%";

    return result + display.str();
}

void NCursesDisplay::DisplaySystem(System &system, WINDOW *window)
{
    int row{0};

    mvwprintw(window, ++row, 2, "%s",
              ("OS: " + system.OperatingSystem()).c_str());

    mvwprintw(window, ++row, 2, "%s",
              ("Kernel: " + system.Kernel()).c_str());

    mvwprintw(window, ++row, 2, "CPU: ");

    wattron(window, COLOR_PAIR(1));

    wprintw(window, "%s",
            ProgressBar(system.Cpu().Utilization()).c_str());

    wattroff(window, COLOR_PAIR(1));

    mvwprintw(window, ++row, 2, "Memory: ");

    wattron(window, COLOR_PAIR(1));

    wmove(window, row, 10);

    wprintw(window, "%s",
            ProgressBar(system.MemoryUtilization()).c_str());

    wattroff(window, COLOR_PAIR(1));

    mvwprintw(window, ++row, 2, "%s",
              ("Total Processes: " +
               to_string(system.TotalProcesses())).c_str());

    mvwprintw(window, ++row, 2, "%s",
              ("Running Processes: " +
               to_string(system.RunningProcesses())).c_str());

    mvwprintw(window, ++row, 2, "%s",
              ("Up Time: " +
               Format::ElapsedTime(system.UpTime())).c_str());

    wrefresh(window);
}

void NCursesDisplay::DisplayProcesses(
    std::vector<Process> &processes,
    WINDOW *window,
    int n)
{
    int row{0};

    int const pid_column{2};
    int const user_column{9};
    int const cpu_column{16};
    int const ram_column{26};
    int const time_column{35};
    int const command_column{46};

    wattron(window, COLOR_PAIR(2));

    mvwprintw(window, ++row, pid_column, "PID");
    mvwprintw(window, row, user_column, "USER");
    mvwprintw(window, row, cpu_column, "CPU[%%]");
    mvwprintw(window, row, ram_column, "RAM[MB]");
    mvwprintw(window, row, time_column, "TIME+");
    mvwprintw(window, row, command_column, "COMMAND");

    wattroff(window, COLOR_PAIR(2));

    int const num_processes =
        std::min(static_cast<int>(processes.size()), n);

    int const command_width =
        std::max(0, getmaxx(window) - command_column - 1);

    for (int i = 0; i < num_processes; ++i)
    {
        mvwprintw(window, ++row, pid_column, "%s",
                  to_string(processes[i].Pid()).c_str());

        mvwprintw(window, row, user_column, "%s",
                  processes[i].User().c_str());

        float cpu = processes[i].CpuUtilization() * 100;

        mvwprintw(window, row, cpu_column, "%s",
                  to_string(cpu).substr(0, 4).c_str());

        mvwprintw(window, row, ram_column, "%s",
                  processes[i].Ram().c_str());

        mvwprintw(window, row, time_column, "%s",
                  Format::ElapsedTime(processes[i].UpTime()).c_str());

        mvwprintw(window, row, command_column, "%s",
                  processes[i].Command()
                      .substr(0, command_width).c_str());
    }
}

void NCursesDisplay::Display(System &system, int n)
{
    stop_requested = 0;

    // Register signal handlers.
    std::signal(SIGINT, HandleStopSignal);
    std::signal(SIGTERM, HandleStopSignal);

    initscr();
    noecho();
    cbreak();
    start_color();

    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    int x_max = getmaxx(stdscr);

    if (x_max <= 1 || n < 0)
    {
        endwin();
        return;
    }

    WINDOW *system_window = newwin(9, x_max - 1, 0, 0);

    // Check the system window before using getmaxy().
    if (system_window == nullptr)
    {
        endwin();
        return;
    }

    // Start process window immediately after the system window.
    WINDOW *process_window =
        newwin(3 + n, x_max - 1,
               getmaxy(system_window), 0);

    if (process_window == nullptr)
    {
        delwin(system_window);
        endwin();
        return;
    }

    while (!stop_requested)
    {
        init_pair(1, COLOR_BLUE, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);

        box(system_window, 0, 0);
        box(process_window, 0, 0);

        DisplaySystem(system, system_window);

        DisplayProcesses(system.Processes(), process_window, n);

        wrefresh(system_window);
        wrefresh(process_window);
        refresh();

        // Press Q or q to exit.
        int key = getch();

        if (key == 'q' || key == 'Q')
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Clean up ncurses windows.
    delwin(system_window);
    delwin(process_window);

    endwin();

    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
}
