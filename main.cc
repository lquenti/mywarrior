#include <cctype>
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

void init_nc() {
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
}

void write_out_ndjson(
    const std::chrono::system_clock::time_point &start,
    const std::chrono::system_clock::time_point &end) {
  auto json = nlohmann::json{
    {"start", timepoint_to_iso<std::chrono::system_clock>(start)},
    {"end", timepoint_to_iso<std::chrono::system_clock>(end)}
  };
  auto json_str{json.dump()};
  debug_print(json_str);
  std::ofstream ofs(TRACK_FILE, std::ios_base::app);
  ofs << json_str << std::endl;
  ofs.close();
}

void track_main(std::uint64_t pomodoro_count) {
  std::signal(SIGINT, signal_handler);
  debug_print("Pomodoro count: ", pomodoro_count);

  std::uint64_t total_seconds{pomodoro_count*60*25};
  debug_print("Total seconds: ", total_seconds);
  std::cout << "Enter to stop early" << std::endl;

  init_nc();

  int input{1337};
  uint64_t secs;
  auto start{std::chrono::system_clock::now()};
  while (!(input == '\n' || input == 'q' || sigint_recieved)) {
    secs = seconds_since(start);

    // clear screen
    nc::clear();
    std::stringstream first_line;
    bool time_is_up = secs >= total_seconds;
    if (time_is_up) {
      first_line << "Time over since " << format_seconds(secs-total_seconds);
    } else {
      first_line << "Time remaining: " << format_seconds(total_seconds-secs);
    }
    nc::mvprintw(0, 0, "%s", first_line.str().c_str());
    nc::mvprintw(2, 0, "q or enter to stop timer");
    nc::mvprintw(3, 0, "Debug: Last Input '%d'", input);
    nc::refresh(); // refresh includes "flush out"
    nc::napms(500); // sleep

    // equal to getch(), but without macros
    input = nc::wgetch(nc::stdscr);
    if (input == '\n' || input == 'q') {
      break;
    }

    // if time is already up: notify the user every 10 seconds
    if (time_is_up && ((secs-total_seconds)%10)==0) {
      play_sound();
    }
  }

  nc::endwin();

  auto end{std::chrono::system_clock::now()};
  auto delta{std::chrono::duration_cast<std::chrono::seconds>(end-start)};
  std::cout << "Successfully worked for " << delta.count() << " seconds!"
    << std::endl;

  write_out_ndjson(start, end);
  debug_print("end track");
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

std::string get_current_date_string() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  std::tm local_tm = *std::localtime(&now_c);
  std::ostringstream date_stream;
  date_stream << std::put_time(&local_tm, "%Y-%m-%d");
  return date_stream.str();
}

// Assuming
// YYYY-mm-dd and hh-mm
// NOTICE NO SECONDS
std::chrono::system_clock::time_point parse_datetime(const std::string& date_str,
    const std::string& time_str) {
    std::tm tm = {};
    std::istringstream ss(date_str + " " + time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return tp;
}

// Assuming yyyy-mm-dd
bool validate_date(std::string maybe_date) {
  // allow for left_whitespace
  size_t left_idx{0};
  while (left_idx < maybe_date.size() && std::isspace(maybe_date[left_idx])) {
    left_idx++;
  }
  // TODO continue, also check that nothing is afterwards
  return false;
}


void add_main() {
  // was it today
  std::string buf;
  std::cout << "Was it today? (y/n)" << std::endl;
  std::getline(std::cin, buf); // used to get rid of newline
  bool was_it_today;
  char answer = std::tolower(buf[0]);
  if (answer == 'y') {
    was_it_today = true;
  } else if (answer == 'n') {
    was_it_today = false;
  } else {
    was_it_today = false;
    std::cout << "Not understood... I am assuming it was today" << std::endl;
  }

  std::string start_date, end_date, start_time, end_time;

  // get start and end date
  if (!was_it_today) {
    // TODO make it run iteratively if it does not fit the format
    std::cout << "Enter start date (YYYY-mm-dd): ";
    std::getline(std::cin, start_date);
    std::cout << "Enter end date (YYYY-mm-dd): ";
    std::getline(std::cin, end_date);
  } else {
    // Automatically set today's date
    start_date = end_date = get_current_date_string();
  }
  debug_print("Start Date ", start_date);
  debug_print("End Date ", end_date);

  // get start and end time
  // TODO make it run iteratively if it does not fit the format
  std::cout << "Enter start time (hh:mm): ";
  std::getline(std::cin, start_time);
  std::cout << "Enter end time (hh:mm): ";
  std::getline(std::cin, end_time);

  // write out
  auto start_tp = parse_datetime(start_date, start_time);
  auto end_tp = parse_datetime(end_date, end_time);
  write_out_ndjson(start_tp, end_tp);
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

  argparse::ArgumentParser add_command("add");
  add_command.add_description("Add manually tracked time");

  program.add_subparser(track_command);
  program.add_subparser(report_command);
  program.add_subparser(add_command);

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
  } else if (program.is_subcommand_used("add")) {
    debug_print("Starting Add");
    add_main();
  } else {
    std::cerr << program << std::endl;
    return EXIT_FAILURE;
  }
}
