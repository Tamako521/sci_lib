#ifndef PROCESS_RESPORTER_HPP
#define PROCESS_RESPORTER_HPP

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

class ProcessReporter {
private:
    using Clock = std::chrono::steady_clock;

    std::string stage_;
    size_t total_ = 0;
    size_t next_percent_ = 0;
    size_t min_step_count_ = 100000;
    size_t last_count_ = 0;
    bool finished_ = false;
    Clock::time_point start_time_;

    double elapsed_seconds() const {
        const auto now = Clock::now();
        return std::chrono::duration<double>(now - start_time_).count();
    }

    static void print_seconds(double seconds) {
        if (seconds < 60.0) {
            std::cout << seconds << "s";
            return;
        }

        const size_t total_seconds = static_cast<size_t>(seconds);
        const size_t minutes = total_seconds / 60;
        const size_t rest_seconds = total_seconds % 60;
        std::cout << minutes << "m" << rest_seconds << "s";
    }

public:
    explicit ProcessReporter(std::string stage, size_t total, size_t min_step_count = 100000)
        : stage_(std::move(stage))
        , total_(total)
        , min_step_count_(min_step_count)
        , start_time_(Clock::now())
    {
        std::cout << "[INFO] " << stage_ << "开始";
        if (total_ > 0)
            std::cout << ": total=" << total_;
        std::cout << std::endl;
    }

    void report(size_t current, const std::string& extra = "") {
        if (finished_)
            return;

        if (total_ == 0) {
            if (current - last_count_ < min_step_count_ && current != 0)
                return;
            last_count_ = current;
            std::cout << "[INFO] " << stage_ << ": current=" << current;
            std::cout << ", elapsed=";
            print_seconds(elapsed_seconds());
            if (!extra.empty())
                std::cout << ", " << extra;
            std::cout << std::endl;
            return;
        }

        if (current > total_)
            current = total_;

        const size_t percent = current * 100 / total_;
        const bool reach_percent = percent >= next_percent_;
        const bool reach_count = current - last_count_ >= min_step_count_;
        const bool reach_end = current == total_;

        if (!reach_percent && !reach_count && !reach_end)
            return;

        last_count_ = current;
        next_percent_ = percent + 5;

        std::cout << "[INFO] " << stage_ << ": " << percent << "% ("
                  << current << "/" << total_ << ")";
        const double elapsed = elapsed_seconds();
        std::cout << ", elapsed=";
        print_seconds(elapsed);
        if (current > 0 && current < total_) {
            const double estimated_total = elapsed * static_cast<double>(total_) / static_cast<double>(current);
            const double remaining = estimated_total - elapsed;
            std::cout << ", eta=";
            print_seconds(remaining);
        }
        if (!extra.empty())
            std::cout << ", " << extra;
        std::cout << std::endl;

        if (reach_end)
            finished_ = true;
    }

    void finish(const std::string& extra = "") {
        if (finished_)
            return;

        if (total_ > 0)
            report(total_, extra);
        else {
            std::cout << "[INFO] " << stage_ << "完成";
            std::cout << ": elapsed=";
            print_seconds(elapsed_seconds());
            if (!extra.empty())
                std::cout << ", " << extra;
            std::cout << std::endl;
            finished_ = true;
        }
    }
};

#endif
