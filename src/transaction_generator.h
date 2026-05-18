#ifndef TRANSACTION_GENERATOR_H
#define TRANSACTION_GENERATOR_H

#include <vector>
#include <cstdint>
#include <random>
#include <string>
#include <functional>

/*
 * TransactionGenerator
 *
 * Simulates transaction arrivals on the network using a Poisson process.
 * Generates two types of transactions:
 *   1. Regular transactions — transfers between users
 *   2. Coinbase transactions — mining rewards (source of new money, no sender)
 *
 * Inter-arrival times follow an exponential distribution (Poisson process).
 */

// Simplified transaction struct for generation (before protobuf serialization)
struct GeneratedTransaction {
    std::string sender_public_key;     // empty for coinbase transactions
    std::string recipient_public_key;
    uint64_t amount;
    uint64_t timestamp;
    bool is_coinbase;                  // true = mining reward, false = regular transfer
};

class TransactionGenerator {
public:
    // rate = average number of transactions per second (lambda for Poisson)
    // coinbase_probability = chance that a generated transaction is a mining reward (0.0 to 1.0)
    // reward_amount = how many coins a coinbase transaction creates
    TransactionGenerator(double rate, double coinbase_probability, uint64_t reward_amount);

    // Generate the next transaction and return it.
    // Also returns the wait time (in seconds) before this transaction should be sent.
    GeneratedTransaction generate_next(double& wait_time_seconds);

    // Generate a batch of transactions for a given time window (in seconds).
    // Returns all transactions that would arrive during that window.
    std::vector<GeneratedTransaction> generate_for_duration(double duration_seconds);

    // Getters / Setters
    double get_rate() const;
    void set_rate(double rate);

    double get_coinbase_probability() const;
    void set_coinbase_probability(double probability);

private:
    double rate_;                       // lambda: avg transactions per second
    double coinbase_probability_;       // chance of coinbase vs regular transaction
    uint64_t reward_amount_;            // coins created per mining reward

    std::mt19937 rng_;                  // random number generator
    std::exponential_distribution<double> arrival_dist_;  // inter-arrival times
    std::uniform_real_distribution<double> coinbase_dist_; // decides coinbase vs regular

    // Helpers
    GeneratedTransaction create_regular_transaction();
    GeneratedTransaction create_coinbase_transaction(const std::string& miner_key);
    std::string generate_fake_key();    // generates a placeholder public key for simulation
    uint64_t current_timestamp();
};

#endif // TRANSACTION_GENERATOR_H
