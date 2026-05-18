cmake_minimum_required(VERSION 3.15)
project(grpc_bitcoin_prototype CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Protobuf and gRPC packages installed on your system
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Include directories so the compiler can find our header files (.h)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Define all the source files that need to be compiled
set(SOURCES
    src/main.cpp
    src/node.cpp
    src/difficulty_adjuster.cpp
    src/transaction_generator.cpp
    src/grpc_server.cpp
    src/grpc_client.cpp
)

# Define the generated Protobuf and gRPC files
set(PROTO_SOURCES
    src/block.pb.cc
    src/block.grpc.pb.cc
    src/node.pb.cc
    src/node.grpc.pb.cc
    src/node_service.pb.cc
    src/node_service.grpc.pb.cc
    src/transaction.pb.cc
    src/transaction.grpc.pb.cc
)

# Tell the compiler to build an executable named "bitcoin_node"
add_executable(bitcoin_node ${SOURCES} ${PROTO_SOURCES})

# Tell the linker to wire up our code with Google's gRPC and Protobuf libraries
target_link_libraries(bitcoin_node
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
)
