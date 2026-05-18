#ifndef DIFFICULTY_ADJUSTER_H
#define DIFFICULTY_ADJUSTER_H

#include <vector>
#include <cstdint>

/*
 * DifficultyAdjuster
 * 
 * Tracks the last N solve times for Proof of Work and adjusts the
 * difficulty (number of leading zeros required in the block hash)
 * based on whether mining is getting faster or slower.
 *
 * If the average solve time decreases → increase difficulty (more leading zeros)
 * If the average solve time increases → decrease difficulty (fewer leading zeros)
 */

class DifficultyAdjuster {
public:
    // Constructor: set initial difficulty and how many timings to track
    DifficultyAdjuster(uint32_t initial_difficulty, size_t window_size = 10);

    // Record a new solve time (in seconds)
    void add_solve_time(double solve_time_seconds);

    // Get the current average of the last N solve times
    double get_average_solve_time() const;

    // Get the previous average (before the last adjustment)
    double get_previous_average() const;

    // Get the current difficulty (number of leading zeros required)
    uint32_t get_difficulty() const;

    // Set the difficulty manually
    void set_difficulty(uint32_t difficulty);

    // Takes the last N+1 block timestamps from the chain,
    // computes solve times from consecutive timestamps,
    // and adjusts difficulty automatically. Returns the new difficulty.
    uint32_t compute_from_chain(const std::vector<uint64_t>& block_timestamps);

    // Evaluate whether difficulty should change based on recent solve times.
    // Called internally by compute_from_chain.
    // Returns the updated difficulty.
    uint32_t adjust_difficulty();

private:
    uint32_t difficulty_;             // current number of leading zeros required
    size_t window_size_;              // how many recent timings to track (default 10)
    std::vector<double> solve_times_; // rolling window of recent solve times
    double previous_average_;         // the average from the last adjustment
    double target_time_;              // ideal solve time in seconds (e.g., 10.0)
};

#endif // DIFFICULTY_ADJUSTER_H
