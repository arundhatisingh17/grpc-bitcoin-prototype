#ifndef NODE_H
#define NODE_H

#include <string>
#include <vector>
#include <cstdint>
#include "difficulty_adjuster.h"
#include "transaction_generator.h"

/*
 * Node
 *
 * Represents a single node (miner) in the Bitcoin network.
 * Owns the local blockchain, mempool, and difficulty adjuster.
 * This is the central class that ties all modules together.
 */

// C++ representation of a Block (mirrors block.proto)
struct BlockData {
    std::string prev_block_hash;
    std::string merkle_root;
    uint64_t timestamp;
    uint32_t difficulty_target;
    uint64_t nonce;
    std::vector<std::string> transaction_ids;
    std::vector<GeneratedTransaction> transactions;
};

// Info about a connected peer node
struct PeerInfo {
    std::string node_id;
    std::string address;    // IP:port
};

class Node {
public:
    // Create a node with an ID, network address, keys, and starting difficulty
    Node(const std::string& node_id, const std::string& address, 
         const std::string& pub_key, const std::string& priv_key, 
         uint32_t initial_difficulty);

    // --- Mempool ---
    // Add a transaction to the mempool (pending, unconfirmed)
    bool add_transaction_to_mempool(const GeneratedTransaction& tx);

    // Get all transactions currently in the mempool
    std::vector<GeneratedTransaction> get_mempool() const;

    // --- Mining ---
    // Mine a new block from the current mempool transactions.
    // Performs Proof of Work and adds the block to the chain.
    // Returns true if a block was successfully mined.
    bool mine_block();

    // --- Chain ---
    // Get the full blockchain
    std::vector<BlockData> get_chain() const;

    // Get the latest block
    BlockData get_latest_block() const;

    // Get the current chain length
    size_t get_chain_length() const;

    // Add a block received from another node (after validation)
    bool add_block(const BlockData& block);

    // Resolve a fork: compare an incoming chain against ours.
    // If the incoming chain is longer, find the fork point and replace.
    // Returns true if our chain was replaced.
    bool resolve_fork(const std::vector<BlockData>& incoming_chain);

    // --- Validation ---
    // Validate a block (check PoW, check transactions)
    bool validate_block(const BlockData& block) const;

    // --- Peers ---
    void add_peer(const PeerInfo& peer);
    std::vector<PeerInfo> get_peers() const;

    // --- Info ---
    std::string get_node_id() const;
    std::string get_address() const;
    uint32_t get_difficulty() const;
    std::string get_public_key() const;

    // Simple SHA-256 placeholder exposed for the visual race simulation
    std::string sha256(const std::string& data) const;

private:
    std::string node_id_;
    std::string address_;
    std::string public_key_;
    std::string private_key_;

    std::vector<BlockData> chain_;                  // the local blockchain
    std::vector<GeneratedTransaction> mempool_;     // unconfirmed transactions
    std::vector<PeerInfo> peers_;                   // known peer nodes
    DifficultyAdjuster difficulty_adjuster_;        // adjusts PoW difficulty

    // --- Private helpers ---
    // Create the genesis block (first block in the chain, hardcoded)
    BlockData create_genesis_block();

    // Hash a block's header fields into a single string
    std::string hash_block(const BlockData& block) const;

    // Compute the merkle root from a list of transactions
    std::string compute_merkle_root(const std::vector<GeneratedTransaction>& transactions) const;

    // Check if a hash meets the difficulty target (has enough leading zeros)
    bool meets_difficulty(const std::string& hash, uint32_t difficulty) const;

    // Walk both chains from the start and find where they diverge
    size_t find_fork_point(const std::vector<BlockData>& incoming_chain) const;
};

#endif // NODE_H
