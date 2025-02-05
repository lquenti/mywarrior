#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace nc {
  #include <ncurses.h>
};

#include "argparse.hpp"
#include "json.hpp"

bool sigint_recieved{false};

const std::string TRACK_FILE{"mywarrior.ndjson"};

template <typename ...Args>
void debug_print(Args &&...args) {
#ifndef NDEBUG
  auto now{std::chrono::system_clock::now()};
  auto now_tt{std::chrono::system_clock::to_time_t(now)};
  std::cerr << "[DEBUG "
    << std::put_time(std::localtime(&now_tt), "%Y-%m-%d %H:%M:%S")
    << "] ";
  ((std::cerr << std::forward<Args>(args) << " " ), ...);
  std::cerr << std::endl;
#else
/* noop intended */
#endif
}

template <typename Num>
std::string format_seconds(Num total_seconds) {
  Num hours{total_seconds/3600},
    mins{(total_seconds%3600)/60},
    secs{total_seconds%60};
  std::ostringstream oss{};
  if (hours) {
    oss << std::setw(2) << std::setfill('0') << hours << ":";
  }
  if (mins) {
    oss << std::setw(2) << std::setfill('0') << mins << ":";
  }
  oss << std::setw(2) << std::setfill('0') << secs;
  return oss.str();
}

template <typename Clock>
std::string timepoint_to_iso(const typename Clock::time_point &tp) {
  auto tt{Clock::to_time_t(tp)};
  std::ostringstream oss;
  oss << std::put_time(std::localtime(&tt), "%Y-%m-%dT%H:%M:%S");
  return oss.str();
}

/* TODO replace me with SDL or sth serious */
void play_sound() {
  system("play -nq -t alsa synth 0.5 sine 440 vol 0.5");
}

void signal_handler(int signal) {
  if (signal == SIGINT) {
    sigint_recieved = true;
  }
}

std::uint64_t seconds_since(const std::chrono::system_clock::time_point &tp) {
  return std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now() - tp
  ).count();
}

int track_main(std::uint64_t pomodoro_count) {
  std::signal(SIGINT, signal_handler);
  debug_print("Pomodoro count: ", pomodoro_count);

  std::uint64_t total_seconds{pomodoro_count*60*25};
  debug_print("Total seconds: ", total_seconds);
  std::cout << "Enter to stop early" << std::endl;


  // TODO refactor out
  // init ncurses
  nc::initscr();
  // Don't echo keyboard input
  nc::noecho();
  // Read input on a character basis
  nc::cbreak();
  // hide cursor
  nc::curs_set(0);
  // Make getch non blocking
  nc::nodelay(nc::stdscr, TRUE);

  int input{1337};
  uint64_t secs;
  auto start{std::chrono::system_clock::now()};
  while (!(input == '\n' || input == 'q' || sigint_recieved)) {
    secs = seconds_since(start);
    // clear screen
    nc::clear();
    std::stringstream first_line;
    if (secs < total_seconds) {
      first_line << "Time remaining: " << format_seconds(total_seconds-secs);
    } else {
      first_line << "Time over since " << format_seconds(secs-total_seconds);
    }
    nc::mvprintw(0, 0, "%s", first_line.str().c_str());
    nc::mvprintw(2, 0, "q or enter to stop timer");
    nc::mvprintw(3, 0, "Debug: Last Input '%d'", input);
    // refresh (and flush out) the screen
    nc::refresh();
    // sleep for .5 second
    nc::napms(500);
    // equal to getch(), but without macros
    input = nc::wgetch(nc::stdscr);
    if (input == '\n' || input == 'q') {
      break;
    }

    // notify the user every 10 seconds if time is already up
    if (secs >= total_seconds && ((secs-total_seconds)%10)==0) {
      play_sound();
    }
  }

  // Clean up ncurses
  nc::endwin();

  auto end{std::chrono::system_clock::now()};
  auto delta{std::chrono::duration_cast<std::chrono::seconds>(end-start)};
  std::cout << "Successfully worked for " << delta.count() << " seconds!"
    << std::endl;

  auto json = nlohmann::json{
    {"start", timepoint_to_iso<std::chrono::system_clock>(start)},
    {"end", timepoint_to_iso<std::chrono::system_clock>(end)}
  };
  auto json_str{json.dump()};
  debug_print(json_str);
  std::ofstream ofs(TRACK_FILE, std::ios_base::app);
  ofs << json_str << std::endl;
  ofs.close();
  debug_print("end");
  return EXIT_SUCCESS;
}

void report_main() {
  std::vector<nlohmann::json> xs;
  std::ifstream ifs(TRACK_FILE);
  std::string buf;
  while (std::getline(ifs, buf)) {
    xs.emplace_back(nlohmann::json::parse(buf));
  }
  // TODO continue here
}

int main(int argc, char **argv) {
  argparse::ArgumentParser program("mywarrior", "0.0.1");

  argparse::ArgumentParser track_command("track");
  track_command.add_description("Tracks pomodori");
  track_command.add_argument("pomodori")
    .help("The amount of pomodori (25min) done in a row")
    .scan<'i', int>();

  argparse::ArgumentParser report_command("report");
  report_command.add_description("provides report of recent work");

  program.add_subparser(track_command);
  program.add_subparser(report_command);

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return EXIT_FAILURE;
  }

  if (program.is_subcommand_used("track")) {
    debug_print("Starting Track");
    int pomodori{track_command.get<int>("pomodori")};
    track_main(pomodori);
    return EXIT_SUCCESS;
  } else if (program.is_subcommand_used("report")) {
    debug_print("Starting Report");
    report_main();
    return EXIT_SUCCESS;
  } else {
    std::cerr << program << std::endl;
    return EXIT_FAILURE;
  }
}
