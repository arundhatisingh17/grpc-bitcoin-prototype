#include "grpc_client.h"
#include "grpc_server.h"
#include "node.h"
#include "transaction_generator.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

// ANSI Color Codes
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";
const std::string WHITE = "\033[37m";

void print_banner(const std::string &msg) {
  std::cout << "\n"
            << MAGENTA
            << "=========================================================="
            << RESET << "\n";
  std::cout << MAGENTA << "|| " << WHITE << msg << MAGENTA << " ||" << RESET
            << "\n";
  std::cout << MAGENTA
            << "=========================================================="
            << RESET << "\n\n";
}

void print_tx(const GeneratedTransaction &tx) {
  if (tx.is_coinbase) {
    std::cout << GREEN << "  [COINBASE] Network generated " << tx.amount
              << " BTC -> " << tx.recipient_public_key << RESET << "\n";
  } else {
    std::cout << YELLOW << "  [TRANSFER] " << tx.sender_public_key << " sent "
              << tx.amount << " BTC -> " << tx.recipient_public_key << RESET
              << "\n";
  }
}

// Visual simulation of a mining race
Node *simulate_mining_race(std::vector<Node *> &miners,
                           const std::string &prev_hash, uint32_t difficulty) {
  bool solved = false;
  uint64_t nonce = 0;
  Node *winner = nullptr;

  std::string target_prefix(difficulty, '0');

  std::cout << WHITE << "TARGET (Must start with): " << CYAN << target_prefix
            << "..." << RESET << "\n\n";

  while (!solved) {
    for (auto *miner : miners) {
      // Create the string to guess: PubKey + PrevHash + Nonce
      std::string attempt =
          miner->get_public_key() + prev_hash + std::to_string(nonce);
      std::string hash = miner->sha256(attempt);

      // Only print a fraction of guesses so the terminal doesn't freeze and
      // looks cool
      if (nonce % 15 == 0) {
        std::cout << "  " << WHITE << miner->get_public_key() << " + "
                  << prev_hash.substr(0, 8) << "... + " << std::setw(6) << nonce
                  << " = "
                  << (miner->get_node_id().find("Cartel") != std::string::npos
                          ? RED
                          : BLUE)
                  << hash << RESET << "\n";
        std::this_thread::sleep_for(
            std::chrono::milliseconds(75)); // Slower visual delay
      }

      // Check if they won!
      if (hash.substr(0, difficulty) == target_prefix) {
        solved = true;
        winner = miner;

        std::cout << GREEN << "\n🏆 WINNER FOUND! 🏆\n" << RESET;
        std::cout << GREEN << "  Node: " << winner->get_node_id() << "\n"
                  << RESET;
        std::cout << GREEN << "  Winning Hash: " << hash << "\n" << RESET;
        std::cout << GREEN << "  Winning Nonce: " << nonce << "\n" << RESET;
        break;
      }
    }
    nonce++;
  }
  return winner;
}

int main() {
  print_banner("THE BITCOIN GRAND FINALE SIMULATION");

  // ACT 1: Setup 10 Nodes (Keys, Ports, Roles)
  std::cout << YELLOW << "Act 1: Initializing 10 Bitcoin Nodes" << RESET
            << "\n";

  std::vector<std::unique_ptr<Node>> all_nodes;
  std::vector<std::thread> servers;

  // 3 Honest Nodes, 6 Cartel Nodes, 1 Idle Node
  for (int i = 0; i < 10; i++) {
    std::string name = (i < 3)   ? "Honest_" + std::to_string(i)
                       : (i < 9) ? "Cartel_" + std::to_string(i)
                                 : "Idle_9";

    std::string port = "0.0.0.0:" + std::to_string(50050 + i);
    std::string pub = "PUB_" + name;
    std::string priv = "PRIV_" + name; // Secret in C++ memory

    all_nodes.push_back(std::make_unique<Node>(name, port, pub, priv, 1));
  }

  // Start gRPC servers
  for (int i = 0; i < 10; i++) {
    servers.push_back(std::thread([&all_nodes, i]() {
      run_grpc_server(*all_nodes[i], all_nodes[i]->get_address());
    }));
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << GREEN << "10 Nodes Online and Listening" << RESET << "\n";

  // Setup Actors
  std::string malice_pub = all_nodes[3]->get_public_key();
  std::string merchant_pub = "MERCHANT";

  // Give Malice 10 BTC via a Coinbase transaction
  GeneratedTransaction coinbase;
  coinbase.is_coinbase = true;
  coinbase.sender_public_key = "";
  coinbase.recipient_public_key = malice_pub;
  coinbase.amount = 10;
  coinbase.timestamp = 1000;

  print_tx(coinbase);
  all_nodes[0]->add_transaction_to_mempool(coinbase);
  all_nodes[0]->mine_block(); // Honest node mines it into Genesis Block
  std::string block1_hash = all_nodes[0]->get_chain().back().prev_block_hash;

  std::this_thread::sleep_for(
      std::chrono::seconds(4)); // Pause for viewer to read

  // ========================================================================
  // ACT 2: The Mining Race & Eventual Consistency
  // ========================================================================
  print_banner(
      "ACT 2: Nodes working towards solving PoW puzzle to mine block 2");

  // Create a regular transaction
  GeneratedTransaction reg_tx;
  reg_tx.is_coinbase = false;
  reg_tx.sender_public_key = "PERSON_A";
  reg_tx.recipient_public_key = "PERSON_B";
  reg_tx.amount = 5;
  reg_tx.timestamp = 1001;
  print_tx(reg_tx);

  // Setup the racers
  std::vector<Node *> racers;
  for (int i = 0; i < 6; i++)
    racers.push_back(all_nodes[i].get());

  std::cout << "\n"
            << YELLOW << "6 Nodes are competing to solve the PoW puzzle..."
            << RESET << "\n";
  Node *winner =
      simulate_mining_race(racers, block1_hash, 1); // Difficulty 1 (0...)

  // The winner packages the block
  winner->add_transaction_to_mempool(reg_tx);
  winner->mine_block();

  std::cout << "\n"
            << YELLOW << "--- Eventual Consistency ---" << RESET << "\n";
  std::cout
      << WHITE
      << "Node 9 (Idle) was asleep during the race. Let's check its chain:"
      << RESET << "\n";
  std::cout << "  Node 9 Chain Length: " << all_nodes[9]->get_chain_length()
            << "\n";

  std::cout << CYAN << "[Node 9] Syncing from Winner (" << winner->get_node_id()
            << ")..." << RESET << "\n";
  NodeClient client_9(winner->get_address());
  auto missing = client_9.get_blockchain(all_nodes[9]->get_chain_length());
  for (const auto &b : missing)
    all_nodes[9]->add_block(b);

  std::cout << GREEN << "[Node 9] Sync Complete! New Chain Length: "
            << all_nodes[9]->get_chain_length() << RESET << "\n";

  std::this_thread::sleep_for(
      std::chrono::seconds(5)); // Pause for viewer to read

  // ========================================================================
  // ACT 3: The 51% Attack (Double Spend)
  // ========================================================================
  print_banner("ACT 3: The 51% Double Spend Attack");

  std::cout << YELLOW
            << "[Cartel Sync] Cartel Node 3 syncs up with Honest Node 0 before "
               "the attack..."
            << RESET << "\n";
  NodeClient client_3(all_nodes[0]->get_address());
  auto cartel_missing =
      client_3.get_blockchain(all_nodes[3]->get_chain_length());
  for (const auto &b : cartel_missing)
    all_nodes[3]->add_block(b);
  std::cout << GREEN << "[Cartel Sync] Complete! Cartel is at length "
            << all_nodes[3]->get_chain_length() << "\n\n"
            << RESET;

  std::cout
      << WHITE
      << "Malice (Cartel Leader) has 10 BTC. She buys a service from the Merchant."
      << RESET << "\n";

  // TX_A: Malice -> Merchant
  GeneratedTransaction tx_a;
  tx_a.is_coinbase = false;
  tx_a.sender_public_key = malice_pub;
  tx_a.recipient_public_key = merchant_pub;
  tx_a.amount = 10;
  tx_a.timestamp = 2000;
  print_tx(tx_a);

  // Honest nodes see TX_A and mine it into Block 3
  all_nodes[0]->add_transaction_to_mempool(tx_a);
  all_nodes[0]->mine_block();
  std::cout << BLUE << "\n[Honest Nodes] Mined TX_A into Block 3!" << RESET
            << "\n";
  std::cout << GREEN << "[Merchant] Sees Block 3 confirmed. Service is provided!"
            << RESET << "\n";

  std::this_thread::sleep_for(
      std::chrono::seconds(4)); // Pause for dramatic effect

  // MEANWHILE IN SECRET...
  std::cout << RED << "\n[CARTEL (60% Hashpower)] Working in secret." << RESET
            << "\n";
  std::cout
      << RED
      << "Malice creates a fake transaction sending the 10 BTC back to herself!"
      << RESET << "\n";

  // TX_B: Malice -> Malice
  GeneratedTransaction tx_b;
  tx_b.is_coinbase = false;
  tx_b.sender_public_key = malice_pub;
  tx_b.recipient_public_key = malice_pub;
  tx_b.amount = 10;
  tx_b.timestamp = 2001;
  print_tx(tx_b);

  // Cartel mines TX_B into their own Block 3'
  all_nodes[3]->add_transaction_to_mempool(tx_b);
  all_nodes[3]->mine_block(); // Block 3'

  // Cartel mines ONE MORE block (Block 4') to make their chain longer!
  // Add a dummy coinbase to the mempool so mine_block doesn't fail (Bitcoin
  // allows empty blocks, our prototype requires 1 TX)
  GeneratedTransaction dummy;
  dummy.is_coinbase = true;
  dummy.amount = 0;
  dummy.recipient_public_key = malice_pub;
  all_nodes[3]->add_transaction_to_mempool(dummy);
  all_nodes[3]->mine_block(); // Block 4'

  std::cout << RED << "[CARTEL] Secret chain is now length "
            << all_nodes[3]->get_chain_length() << "!" << RESET << "\n";
  std::cout << BLUE << "[Honest] Public chain is length "
            << all_nodes[0]->get_chain_length() << "." << RESET << "\n";

  std::this_thread::sleep_for(
      std::chrono::seconds(40)); // Pause before the strike

  std::cout << "\n"
            << YELLOW
            << "Cartel Broadcasts their longer chain to Honest Node 0..."
            << RESET << "\n";

  // Reorganization happens here
  std::vector<BlockData> cartel_chain = all_nodes[3]->get_chain();
  bool fork_resolved = all_nodes[0]->resolve_fork(cartel_chain);

  if (fork_resolved) {
    std::cout << "\n"
              << RED << "!! CHAIN REORGANIZATION SUCCESSFUL !!" << RESET
              << "\n";
    std::cout << RED
              << "Honest Node dropped Block 3 to follow the longest chain!"
              << RESET << "\n";
    std::cout << RED << "Merchant's transaction (TX_A) was erased from history!"
              << RESET << "\n";
    std::cout << RED
              << "Malice keeps the 10 BTC AND gets the service! (Double Spend "
                 "Complete)"
              << RESET << "\n";
  }

  std::cout << "\n"
            << YELLOW << "(Press Ctrl+C to exit servers)" << RESET << "\n";
  for (auto &t : servers)
    t.join();

  return 0;
}
