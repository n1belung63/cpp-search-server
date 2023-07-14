#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string& out_string, std::ostream& cout = std::cerr)
        : out_string_(out_string), cout_(cout) {
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto duration = end_time - start_time_;
        cout_ << "Operation time: "s << duration_cast<milliseconds>(duration).count() << " ms"s << std::endl;
    }

private:
    const std::string out_string_;
    std::ostream& cout_;
    const Clock::time_point start_time_ = Clock::now();
};