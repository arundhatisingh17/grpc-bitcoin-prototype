#ifndef GRPC_CLIENT_H
#define GRPC_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <grpcpp/grpcpp.h>

#include "node_service.grpc.pb.h"
#include "node_service.pb.h"
#include "transaction.pb.h"
#include "block.pb.h"

#include "node.h"

// Used by a node (or wallet) to communicate with other nodes on the network.
// Each method makes a gRPC call to a remote node's server.

class NodeClient {
public:
  // Connect to a remote node at the given address (e.g., "localhost:50051")
  NodeClient(const std::string& target_address);

  // Send a transaction to the remote node
  bool broadcast_transaction(const GeneratedTransaction& tx,
                             std::string& responder_node_id,
                             std::string& response_message);

  // Send a mined block to the remote node
  bool broadcast_block(const BlockData& block,
                       std::string& responder_node_id,
                       std::string& response_message);

  // Request the blockchain from the remote node (for syncing)
  std::vector<BlockData> get_blockchain(uint64_t start_index);

  // Check if a transaction has been confirmed
  bool get_transaction_status(const std::string& transaction_id,
                              std::string& block_hash,
                              std::string& responder_node_id);

private:
  std::unique_ptr<bitcoin::NodeService::Stub> stub_;
  std::string target_address_;
};

#endif // GRPC_CLIENT_H
