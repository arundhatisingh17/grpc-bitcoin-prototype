#include "grpc_client.h"
#include <iostream>

NodeClient::NodeClient(const std::string& target_address) {
    target_address_ = target_address;

    // Create a gRPC channel (connection) to the remote node
    auto channel = grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials());

    // Create the stub (client-side proxy for calling remote RPCs)
    stub_ = bitcoin::NodeService::NewStub(channel);

    std::cout << "[Client] Connected to " << target_address << std::endl;
}

bool NodeClient::broadcast_transaction(const GeneratedTransaction& tx,
                                        std::string& responder_node_id,
                                        std::string& response_message)
{
    // Build the protobuf request
    bitcoin::BroadcastTransactionRequest request;
    bitcoin::Transaction* proto_tx = request.mutable_transaction();
    proto_tx->set_sender_public_key(tx.sender_public_key);
    proto_tx->set_recipient_public_key(tx.recipient_public_key);
    proto_tx->set_amount(tx.amount);
    proto_tx->set_timestamp(tx.timestamp);

    // Make the RPC call
    bitcoin::BroadcastTransactionResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub_->BroadcastTransaction(&context, request, &response);

    if (!status.ok()) {
        std::cout << "[Client] RPC failed: " << status.error_message() << std::endl;
        return false;
    }

    responder_node_id = response.node_id();
    response_message = response.message();
    return response.accepted();
}

bool NodeClient::broadcast_block(const BlockData& block,
                                  std::string& responder_node_id,
                                  std::string& response_message)
{
    bitcoin::BroadcastBlockRequest request;
    bitcoin::Block* proto_block = request.mutable_block();

    proto_block->set_prev_block_hash(block.prev_block_hash);
    proto_block->set_merkle_root(block.merkle_root);
    proto_block->set_timestamp(block.timestamp);
    proto_block->set_difficulty_target(block.difficulty_target);
    proto_block->set_nonce(block.nonce);

    for (const auto& tx_id : block.transaction_ids) {
        proto_block->add_transaction_ids(tx_id);
    }

    for (const auto& tx : block.transactions) {
        bitcoin::Transaction* proto_tx = proto_block->add_transactions();
        proto_tx->set_sender_public_key(tx.sender_public_key);
        proto_tx->set_recipient_public_key(tx.recipient_public_key);
        proto_tx->set_amount(tx.amount);
        proto_tx->set_timestamp(tx.timestamp);
    }

    bitcoin::BroadcastBlockResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub_->BroadcastBlock(&context, request, &response);

    if (!status.ok()) {
        std::cout << "[Client] RPC failed: " << status.error_message() << std::endl;
        return false;
    }

    responder_node_id = response.node_id();
    response_message = response.message();
    return response.accepted();
}

std::vector<BlockData> NodeClient::get_blockchain(uint64_t start_index) {
    bitcoin::GetBlockchainRequest request;
    request.set_start_index(start_index);

    bitcoin::GetBlockchainResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetBlockchain(&context, request, &response);

    std::vector<BlockData> blocks;

    if (!status.ok()) {
        std::cout << "[Client] GetBlockchain RPC failed: " << status.error_message() << std::endl;
        return blocks;
    }

    // Convert each protobuf Block to our C++ struct
    for (const auto& proto_block : response.blocks()) {
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
            GeneratedTransaction tx;
            tx.sender_public_key = std::string(proto_tx.sender_public_key().begin(),
                                                proto_tx.sender_public_key().end());
            tx.recipient_public_key = std::string(proto_tx.recipient_public_key().begin(),
                                                   proto_tx.recipient_public_key().end());
            tx.amount = proto_tx.amount();
            tx.timestamp = proto_tx.timestamp();
            tx.is_coinbase = tx.sender_public_key.empty();
            block.transactions.push_back(tx);
        }

        blocks.push_back(block);
    }

    std::cout << "[Client] Received " << blocks.size() << " blocks from " 
              << target_address_ << std::endl;

    return blocks;
}

bool NodeClient::get_transaction_status(const std::string& transaction_id,
                                         std::string& block_hash,
                                         std::string& responder_node_id)
{
    bitcoin::GetTransactionStatusRequest request;
    request.set_transaction_id(transaction_id);

    bitcoin::GetTransactionStatusResponse response;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetTransactionStatus(&context, request, &response);

    if (!status.ok()) {
        std::cout << "[Client] GetTransactionStatus RPC failed: " << status.error_message() << std::endl;
        return false;
    }

    block_hash = std::string(response.block_hash().begin(), response.block_hash().end());
    responder_node_id = response.node_id();
    return response.confirmed();
}
