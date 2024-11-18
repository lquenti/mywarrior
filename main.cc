#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

bool stop{false};

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

void timer(std::uint64_t seconds) {
  for (std::uint64_t i{seconds}; i; --i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (stop) {
      break;
    }
    std::cout << format_seconds(i) << std::endl;
  }
}

void signal_handler(int signal) {
  if (signal == SIGINT) {
    stop = true;
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

  std::uint64_t total_seconds{pomodoro_count*60*25};
  debug_print("Total seconds: ", total_seconds);
  std::cout << "Enter to stop early" << std::endl;

  auto start{std::chrono::system_clock::now()};
  std::thread thread(timer, total_seconds);

  std::string buf{};
  std::getline(std::cin, buf);
  stop=true;

  thread.join();
  auto end{std::chrono::system_clock::now()};
  auto delta{std::chrono::duration_cast<std::chrono::seconds>(end-start)};
  std::cout << "Successfully worked for " << delta.count() << " seconds!" 
    << std::endl;
}
