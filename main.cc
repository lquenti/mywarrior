#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "json.hpp"

// stop == stopped by SIGINT or end of timer
// user_ended == newline
// (in case the user continues to work)
bool stop{false};
bool user_ended{false};

const std::string TRACK_FILE{"mywarrior.ndjson"};

template <typename ...Args>
void debug_print(Args &&...args) {
#ifndef NDEBUG
  /* Expands to
   * ((cerr << arg1 << " "), (cerr << arg2 << " "))
   * Standard insures the evaluation order
   * https://en.cppreference.com/w/cpp/language/operator_other#Built-in_comma_operator
   */
  ((std::cerr << std::forward<Args>(args) << " " ), ...);
  std::cerr << std::endl;
#else
/* noop intended */
#endif
}

template <typename T>
std::string format_seconds(T total_seconds) {
  T hours{total_seconds/3600}, 
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

/* TODO replace me with SDL */
void play_sound() {
  system("play -nq -t alsa synth 0.5 sine 440");
}

void remind_user_to_end() {
  while (!user_ended) {
    play_sound();
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
}

void timer(std::uint64_t seconds) {
  for (std::uint64_t i{seconds}; i; --i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (stop) {
      break;
    }
    std::cout << format_seconds(i) << std::endl;
  }
  if (!user_ended) {
    std::thread sound_thread(remind_user_to_end);
    sound_thread.detach();
  }
}

void signal_handler(int signal) {
  if (signal == SIGINT) {
    stop = true;
    user_ended = true;
  }
  std::cout << std::endl << "Press enter to exit!" << std::endl;
}

int main(int argc, char **argv) {
  std::signal(SIGINT, signal_handler);
  /* for now pomodori */
  if (argc != 2) {
    std::cerr << "USAGE: mywarrior (AMOUNT OF POMODORI)" << std::endl;
    return EXIT_FAILURE;
  }
  std::uint64_t pomodoro_count{
    static_cast<std::uint64_t>(std::strtol(argv[1], nullptr, 10))
  };
  debug_print("Pomodoro count: ", pomodoro_count);

  //std::uint64_t total_seconds{pomodoro_count*60*25};
  std::uint64_t total_seconds{3};
  debug_print("Total seconds: ", total_seconds);
  std::cout << "Enter to stop early" << std::endl;

  auto start{std::chrono::system_clock::now()};
  std::thread thread(timer, total_seconds);

  std::string buf{};
  std::getline(std::cin, buf);
  stop=true;
  user_ended=true;

  thread.join();
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
}
