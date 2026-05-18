#include "grpc_server.h"
#include <iostream>

NodeServiceImpl::NodeServiceImpl(Node& node) : node_(node) {}

// --- RPC Implementations ---

grpc::Status NodeServiceImpl::BroadcastTransaction(
    grpc::ServerContext* context,
    const bitcoin::BroadcastTransactionRequest* request,
    bitcoin::BroadcastTransactionResponse* response) {

  GeneratedTransaction tx = proto_to_transaction(request->transaction());
  bool accepted = node_.add_transaction_to_mempool(tx);

  response->set_accepted(accepted);
  response->set_node_id(node_.get_node_id());
  if (accepted) {
    response->set_message("Transaction accepted into mempool");
  } else {
    response->set_message("Transaction rejected (duplicate or invalid)");
  }

  return grpc::Status::OK;
}

grpc::Status NodeServiceImpl::BroadcastBlock(
    grpc::ServerContext* context,
    const bitcoin::BroadcastBlockRequest* request,
    bitcoin::BroadcastBlockResponse* response) {

  BlockData block = proto_to_block(request->block());
  bool accepted = node_.add_block(block);

  response->set_accepted(accepted);
  response->set_node_id(node_.get_node_id());
  if (accepted) {
    response->set_message("Block accepted and added to chain");
  } else {
    response->set_message("Block rejected (validation failed)");
  }

  return grpc::Status::OK;
}

grpc::Status NodeServiceImpl::GetBlockchain(
    grpc::ServerContext* context,
    const bitcoin::GetBlockchainRequest* request,
    bitcoin::GetBlockchainResponse* response) {

  std::vector<BlockData> chain = node_.get_chain();
  uint64_t start_index = request->start_index();

  for (size_t i = start_index; i < chain.size(); i++) {
    bitcoin::Block* proto_block = response->add_blocks();
    *proto_block = block_to_proto(chain[i]);
  }

  std::cout << "[Server " << node_.get_node_id() << "] Sent "
            << (chain.size() - start_index) << " blocks for sync" << std::endl;

  return grpc::Status::OK;
}

grpc::Status NodeServiceImpl::GetTransactionStatus(
    grpc::ServerContext* context,
    const bitcoin::GetTransactionStatusRequest* request,
    bitcoin::GetTransactionStatusResponse* response) {

  std::string tx_id(request->transaction_id().begin(),
                    request->transaction_id().end());

  std::vector<BlockData> chain = node_.get_chain();
  for (const auto& block : chain) {
    for (const auto& stored_tx_id : block.transaction_ids) {
      if (stored_tx_id == tx_id) {
        response->set_confirmed(true);
        response->set_block_hash(block.prev_block_hash);
        response->set_node_id(node_.get_node_id());
        return grpc::Status::OK;
      }
    }
  }

  response->set_confirmed(false);
  response->set_node_id(node_.get_node_id());
  return grpc::Status::OK;
}

// --- Conversion helpers ---

GeneratedTransaction NodeServiceImpl::proto_to_transaction(
    const bitcoin::Transaction& proto_tx) {
  GeneratedTransaction tx;
  tx.sender_public_key = std::string(proto_tx.sender_public_key().begin(),
                                      proto_tx.sender_public_key().end());
  tx.recipient_public_key = std::string(proto_tx.recipient_public_key().begin(),
                                         proto_tx.recipient_public_key().end());
  tx.amount = proto_tx.amount();
  tx.timestamp = proto_tx.timestamp();
  tx.is_coinbase = tx.sender_public_key.empty();
  return tx;
}

bitcoin::Transaction NodeServiceImpl::transaction_to_proto(
    const GeneratedTransaction& tx) {
  bitcoin::Transaction proto_tx;
  proto_tx.set_sender_public_key(tx.sender_public_key);
  proto_tx.set_recipient_public_key(tx.recipient_public_key);
  proto_tx.set_amount(tx.amount);
  proto_tx.set_timestamp(tx.timestamp);
  return proto_tx;
}

BlockData NodeServiceImpl::proto_to_block(const bitcoin::Block& proto_block) {
  BlockData block;
  block.prev_block_hash = std::string(proto_block.prev_block_hash().begin(),
                                       proto_block.prev_block_hash().end());
  block.merkle_root = std::string(proto_block.merkle_root().begin(),
                                   proto_block.merkle_root().end());
  block.timestamp = proto_block.timestamp();
  block.difficulty_target = proto_block.difficulty_target();
  block.nonce = proto_block.nonce();

  for (const auto& tx_id : proto_block.transaction_ids()) {
    block.transaction_ids.push_back(std::string(tx_id.begin(), tx_id.end()));
  }
  for (const auto& proto_tx : proto_block.transactions()) {
    block.transactions.push_back(proto_to_transaction(proto_tx));
  }

  return block;
}

bitcoin::Block NodeServiceImpl::block_to_proto(const BlockData& block) {
  bitcoin::Block proto_block;
  proto_block.set_prev_block_hash(block.prev_block_hash);
  proto_block.set_merkle_root(block.merkle_root);
  proto_block.set_timestamp(block.timestamp);
  proto_block.set_difficulty_target(block.difficulty_target);
  proto_block.set_nonce(block.nonce);

  for (const auto& tx_id : block.transaction_ids) {
    proto_block.add_transaction_ids(tx_id);
  }
  for (const auto& tx : block.transactions) {
    bitcoin::Transaction* proto_tx = proto_block.add_transactions();
    *proto_tx = transaction_to_proto(tx);
  }

  return proto_block;
}

// --- Server startup ---

void run_grpc_server(Node& node, const std::string& address) {
  NodeServiceImpl service(node);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "[Server " << node.get_node_id()
            << "] Listening on " << address << std::endl;

  server->Wait();  // blocks forever, serving requests
}
