#include "node.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>

Node::Node(const std::string &node_id, const std::string &address,
           uint32_t initial_difficulty)
    : difficulty_adjuster_(initial_difficulty) {
  node_id_ = node_id;
  address_ = address;

  // Every blockchain starts with a genesis block
  BlockData genesis = create_genesis_block();
  chain_.push_back(genesis);

  std::cout << "[Node " << node_id_ << "] Initialized at " << address_
            << " with difficulty " << initial_difficulty << std::endl;
}

// Adding transactions to mempool

bool Node::add_transaction_to_mempool(const GeneratedTransaction &tx) {
  // we check timestamp + sender public key + recipient public key to check for
  // duplicate values first check
  for (const auto &existing : mempool_) {
    if (existing.timestamp == tx.timestamp &&
        existing.sender_public_key == tx.sender_public_key &&
        existing.recipient_public_key == tx.recipient_public_key) {
      return false; // duplicate
    }
  }

  mempool_.push_back(tx);
  std::cout << "[Node " << node_id_ << "] Added transaction to mempool"
            << " (mempool size: " << mempool_.size() << ")" << std::endl;
  return true;
}

std::vector<GeneratedTransaction> Node::get_mempool() const { return mempool_; }

bool Node::mine_block() {
  if (mempool_.empty()) {
    std::cout << "[Node " << node_id_ << "] Mempool empty, nothing to mine"
              << std::endl;
    return false;
  }

  // Update difficulty based on recent block timestamps
  std::vector<uint64_t> timestamps;
  for (const auto &block : chain_) {
    timestamps.push_back(block.timestamp);
  }
  difficulty_adjuster_.compute_from_chain(timestamps);

  // Build the candidate block
  BlockData new_block;
  new_block.prev_block_hash = hash_block(chain_.back());
  new_block.transactions = mempool_;
  new_block.merkle_root = compute_merkle_root(mempool_);
  new_block.difficulty_target = difficulty_adjuster_.get_difficulty();
  new_block.nonce = 0;

  // Record transaction IDs
  for (const auto &tx : mempool_) {
    std::string tx_data = tx.sender_public_key + tx.recipient_public_key +
                          std::to_string(tx.amount) +
                          std::to_string(tx.timestamp);
    new_block.transaction_ids.push_back(sha256(tx_data));
  }

  // --- Proof of Work loop ---
  // Keep incrementing the nonce until the block hash meets the difficulty
  // target
  std::cout << "[Node " << node_id_ << "] Mining block #" << chain_.size()
            << " (difficulty: " << new_block.difficulty_target
            << ", transactions: " << new_block.transactions.size() << ")"
            << std::endl;

  auto start_time = std::chrono::steady_clock::now();

  while (true) {
    new_block.timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    std::string block_hash = hash_block(new_block);

    if (meets_difficulty(block_hash, new_block.difficulty_target)) {
      // PoW solved!
      auto end_time = std::chrono::steady_clock::now();
      double solve_time =
          std::chrono::duration<double>(end_time - start_time).count();

      chain_.push_back(new_block);
      mempool_.clear(); // transactions are now in the block

      std::cout << "[Node " << node_id_ << "] Block #" << (chain_.size() - 1)
                << " mined! Nonce: " << new_block.nonce
                << " | Hash: " << block_hash.substr(0, 16) << "..."
                << " | Time: " << solve_time << "s" << std::endl;

      return true;
    }

    new_block.nonce++;
  }
}

// --- Chain ---

std::vector<BlockData> Node::get_chain() const { return chain_; }

BlockData Node::get_latest_block() const { return chain_.back(); }

size_t Node::get_chain_length() const { return chain_.size(); }

bool Node::add_block(const BlockData &block) {
  if (!validate_block(block)) {
    std::cout << "[Node " << node_id_ << "] Rejected invalid block"
              << std::endl;
    return false;
  }

  chain_.push_back(block);
  std::cout << "[Node " << node_id_ << "] Accepted block #"
            << (chain_.size() - 1) << std::endl;
  return true;
}

// --- Validation ---

bool Node::validate_block(const BlockData &block) const {
  // 1. Check that prev_block_hash matches the hash of our latest block
  std::string expected_prev_hash = hash_block(chain_.back());
  if (block.prev_block_hash != expected_prev_hash) {
    std::cout << "[Node " << node_id_ << "] Block rejected: prev_hash mismatch"
              << std::endl;
    return false;
  }

  // 2. Check that the merkle root matches the transactions
  std::string expected_merkle = compute_merkle_root(block.transactions);
  if (block.merkle_root != expected_merkle) {
    std::cout << "[Node " << node_id_
              << "] Block rejected: merkle root mismatch" << std::endl;
    return false;
  }

  // 3. Check that the hash meets the difficulty target (PoW is valid)
  std::string block_hash = hash_block(block);
  if (!meets_difficulty(block_hash, block.difficulty_target)) {
    std::cout << "[Node " << node_id_ << "] Block rejected: PoW invalid"
              << std::endl;
    return false;
  }

  return true;
}

// --- Fork Resolution ---

bool Node::resolve_fork(const std::vector<BlockData>& incoming_chain) {
  // Rule from the paper: always follow the longest chain
  if (incoming_chain.size() <= chain_.size()) {
    std::cout << "[Node " << node_id_ << "] Incoming chain is not longer ("
              << incoming_chain.size() << " vs " << chain_.size()
              << "), keeping ours" << std::endl;
    return false;
  }

  // Find where the chains diverge
  size_t fork_point = find_fork_point(incoming_chain);

  std::cout << "[Node " << node_id_ << "] Fork detected at block #" << fork_point
            << " (our chain: " << chain_.size()
            << ", incoming: " << incoming_chain.size() << ")" << std::endl;

  // Validate every block in the incoming chain from the fork point onward
  for (size_t i = fork_point; i < incoming_chain.size(); i++) {
    // Check PoW is valid for each incoming block
    std::string block_hash = hash_block(incoming_chain[i]);
    if (!meets_difficulty(block_hash, incoming_chain[i].difficulty_target)) {
      std::cout << "[Node " << node_id_
                << "] Incoming chain has invalid PoW at block #"
                << i << ", rejecting" << std::endl;
      return false;
    }

    // Check each block points to the previous one
    if (i > 0) {
      std::string expected_prev = hash_block(incoming_chain[i - 1]);
      if (incoming_chain[i].prev_block_hash != expected_prev) {
        std::cout << "[Node " << node_id_
                  << "] Incoming chain has broken link at block #"
                  << i << ", rejecting" << std::endl;
        return false;
      }
    }
  }

  // Incoming chain is longer and valid — replace ours from the fork point
  // First, put any transactions from our discarded blocks back into the mempool
  for (size_t i = fork_point; i < chain_.size(); i++) {
    for (const auto& tx : chain_[i].transactions) {
      mempool_.push_back(tx);
    }
  }

  // Chop our chain at the fork point
  chain_.erase(chain_.begin() + fork_point, chain_.end());

  // Append the incoming blocks from the fork point onward
  for (size_t i = fork_point; i < incoming_chain.size(); i++) {
    chain_.push_back(incoming_chain[i]);
  }

  std::cout << "[Node " << node_id_ << "] Chain reorganized. New length: "
            << chain_.size() << std::endl;

  return true;
}

size_t Node::find_fork_point(const std::vector<BlockData>& incoming_chain) const {
  // Walk both chains from the beginning, comparing block hashes
  size_t min_length = std::min(chain_.size(), incoming_chain.size());

  for (size_t i = 0; i < min_length; i++) {
    std::string our_hash = hash_block(chain_[i]);
    std::string their_hash = hash_block(incoming_chain[i]);

    if (our_hash != their_hash) {
      return i;  // this is where they diverge
    }
  }

  // No divergence found in the overlapping portion
  return min_length;
}

// --- Peers ---

void Node::add_peer(const PeerInfo &peer) { peers_.push_back(peer); }

std::vector<PeerInfo> Node::get_peers() const { return peers_; }

// --- Info ---

std::string Node::get_node_id() const { return node_id_; }

std::string Node::get_address() const { return address_; }

uint32_t Node::get_difficulty() const {
  return difficulty_adjuster_.get_difficulty();
}

// --- Private helpers ---

BlockData Node::create_genesis_block() {
  BlockData genesis;
  genesis.prev_block_hash =
      "0000000000000000000000000000000000000000000000000000000000000000";
  genesis.merkle_root = "";
  genesis.timestamp = 1231006505;
  genesis.difficulty_target = difficulty_adjuster_.get_difficulty();
  genesis.nonce = 0;
  return genesis;
}

std::string Node::hash_block(const BlockData &block) const {
  // Combine all header fields into a single string, then hash
  std::string header = block.prev_block_hash + block.merkle_root +
                       std::to_string(block.timestamp) +
                       std::to_string(block.difficulty_target) +
                       std::to_string(block.nonce);
  return sha256(header);
}

std::string Node::compute_merkle_root(
    const std::vector<GeneratedTransaction> &transactions) const {
  if (transactions.empty()) {
    return sha256("");
  }

  // Hash each transaction
  std::vector<std::string> hashes;
  for (const auto &tx : transactions) {
    std::string tx_data = tx.sender_public_key + tx.recipient_public_key +
                          std::to_string(tx.amount) +
                          std::to_string(tx.timestamp);
    hashes.push_back(sha256(tx_data));
  }

  // Repeatedly pair and hash until one root remains
  while (hashes.size() > 1) {
    std::vector<std::string> next_level;
    for (size_t i = 0; i < hashes.size(); i += 2) {
      if (i + 1 < hashes.size()) {
        next_level.push_back(sha256(hashes[i] + hashes[i + 1]));
      } else {
        // Odd number: duplicate the last hash
        next_level.push_back(sha256(hashes[i] + hashes[i]));
      }
    }
    hashes = next_level;
  }

  return hashes[0];
}

std::string Node::sha256(const std::string &data) const {
  // Placeholder: uses std::hash until we integrate a real SHA-256 library
  std::hash<std::string> hasher;
  size_t hash_value = hasher(data);

  // Convert to hex string
  std::stringstream ss;
  ss << std::hex << hash_value;

  // Pad to 64 characters to mimic SHA-256 output length
  std::string result = ss.str();
  while (result.length() < 64) {
    result = "0" + result;
  }
  return result;
}

bool Node::meets_difficulty(const std::string &hash,
                            uint32_t difficulty) const {
  // Check that the hash starts with 'difficulty' number of zeros
  for (uint32_t i = 0; i < difficulty && i < hash.length(); i++) {
    if (hash[i] != '0') {
      return false;
    }
  }
  return true;
}
