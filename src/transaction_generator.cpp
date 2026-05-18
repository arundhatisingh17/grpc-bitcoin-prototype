#include "transaction_generator.h"
#include <chrono>
#include <iostream>

TransactionGenerator::TransactionGenerator(double rate, double coinbase_probability, uint64_t reward_amount) {
    rate_ = rate;
    coinbase_probability_ = coinbase_probability;
    reward_amount_ = reward_amount;

    // Seed the random number generator with current time
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    rng_ = std::mt19937(seed);

    // Exponential distribution: inter-arrival time = 1/lambda on average
    // If rate = 5 transactions/sec, average wait = 0.2 seconds between transactions
    arrival_dist_ = std::exponential_distribution<double>(rate_);

    // Uniform [0, 1) for deciding coinbase vs regular
    coinbase_dist_ = std::uniform_real_distribution<double>(0.0, 1.0);
}

GeneratedTransaction TransactionGenerator::generate_next(double& wait_time_seconds) {
    // Sample inter-arrival time from exponential distribution
    wait_time_seconds = arrival_dist_(rng_);

    // Decide: coinbase or regular transaction?
    double roll = coinbase_dist_(rng_);

    if (roll < coinbase_probability_) {
        // Mining reward — new coins enter the system
        std::string miner_key = generate_fake_key();
        return create_coinbase_transaction(miner_key);
    } else {
        // Regular transfer between two users
        return create_regular_transaction();
    }
}

std::vector<GeneratedTransaction> TransactionGenerator::generate_for_duration(double duration_seconds) {
    std::vector<GeneratedTransaction> transactions;
    double elapsed = 0.0;

    while (elapsed < duration_seconds) {
        double wait_time;
        GeneratedTransaction tx = generate_next(wait_time);
        elapsed += wait_time;

        if (elapsed < duration_seconds) {
            transactions.push_back(tx);
        }
    }

    std::cout << "[TransactionGenerator] Generated " << transactions.size()
              << " transactions over " << duration_seconds << " seconds"
              << " (rate=" << rate_ << " tx/sec)" << std::endl;

    return transactions;
}

// --- Getters / Setters ---

double TransactionGenerator::get_rate() const {
    return rate_;
}

void TransactionGenerator::set_rate(double rate) {
    rate_ = rate;
    // Update the distribution with the new rate
    arrival_dist_ = std::exponential_distribution<double>(rate_);
}

double TransactionGenerator::get_coinbase_probability() const {
    return coinbase_probability_;
}

void TransactionGenerator::set_coinbase_probability(double probability) {
    coinbase_probability_ = probability;
}

// --- Private helpers ---

GeneratedTransaction TransactionGenerator::create_regular_transaction() {
    GeneratedTransaction tx;
    tx.sender_public_key = generate_fake_key();
    tx.recipient_public_key = generate_fake_key();

    // Random amount between 1 and 100
    std::uniform_int_distribution<uint64_t> amount_dist(1, 100);
    tx.amount = amount_dist(rng_);

    tx.timestamp = current_timestamp();
    tx.is_coinbase = false;
    return tx;
}

GeneratedTransaction TransactionGenerator::create_coinbase_transaction(const std::string& miner_key) {
    GeneratedTransaction tx;
    tx.sender_public_key = "";          // no sender — coins are created from nothing
    tx.recipient_public_key = miner_key; // miner receives the reward
    tx.amount = reward_amount_;
    tx.timestamp = current_timestamp();
    tx.is_coinbase = true;
    return tx;
}

std::string TransactionGenerator::generate_fake_key() {
    // Generate a random hex string as a placeholder public key
    static const char hex_chars[] = "0123456789abcdef";
    std::string key = "pk_";
    for (int i = 0; i < 16; i++) {
        key += hex_chars[rng_() % 16];
    }
    return key;
}

uint64_t TransactionGenerator::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
}
