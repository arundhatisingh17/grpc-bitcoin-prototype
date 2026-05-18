#include "difficulty_adjuster.h"
#include <algorithm>
#include <iostream>
#include <numeric>

DifficultyAdjuster::DifficultyAdjuster(uint32_t initial_difficulty,
                                       size_t window_size) {
  difficulty_ = initial_difficulty;
  window_size_ = window_size;
  previous_average_ = 0.0;
  target_time_ = 10.0; // default target: 10 seconds per block
}

void DifficultyAdjuster::add_solve_time(double solve_time_seconds) {
  solve_times_.push_back(solve_time_seconds);

  // Keep only the last N timings
  if (solve_times_.size() > window_size_) {
    solve_times_.erase(solve_times_.begin());
  }
}

double DifficultyAdjuster::get_average_solve_time() const {
  if (solve_times_.empty()) {
    return 0.0;
  }

  double sum = std::accumulate(solve_times_.begin(), solve_times_.end(), 0.0);
  return sum / static_cast<double>(solve_times_.size());
}

double DifficultyAdjuster::get_previous_average() const {
  return previous_average_;
}

uint32_t DifficultyAdjuster::get_difficulty() const { return difficulty_; }

void DifficultyAdjuster::set_difficulty(uint32_t difficulty) {
  difficulty_ = difficulty;
}

uint32_t DifficultyAdjuster::adjust_difficulty() {
  // Need at least a full window of timings before adjusting
  if (solve_times_.size() < window_size_) {
    return difficulty_;
  }

  double current_average = get_average_solve_time();

  // Compare current average to previous average (or target time if first
  // adjustment)
  double reference =
      (previous_average_ > 0.0) ? previous_average_ : target_time_;

  if (current_average < reference) {
    // Mining is getting faster → make it harder (more leading zeros)
    difficulty_++;
    std::cout << "[DifficultyAdjuster] Mining faster (avg: " << current_average
              << "s vs ref: " << reference << "s) → difficulty increased to "
              << difficulty_ << std::endl;
  } else if (current_average > reference && difficulty_ > 1) {
    // Mining is getting slower → make it easier (fewer leading zeros)
    difficulty_--;
    std::cout << "[DifficultyAdjuster] Mining slower (avg: " << current_average
              << "s vs ref: " << reference << "s) → difficulty decreased to "
              << difficulty_ << std::endl;
  } else {
    std::cout << "[DifficultyAdjuster] Difficulty unchanged at " << difficulty_
              << std::endl;
  }

  // Store current average as the reference for next adjustment
  previous_average_ = current_average;

  return difficulty_;
}
