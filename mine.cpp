#include <mpi.h>
#include <cstdio>
#include <iostream>
#include "Keccak256.hpp"
#include "uint256_t.h"

void printArr(std::uint8_t* arr, size_t size, std::string label) {
    std::cout << label + ": ";
    for (size_t i=0; i<size; i++) {
        printf("%02x", arr[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
	MPI_Init(&argc, &argv);
	MPI_Comm comm = MPI_COMM_WORLD;
	int p, rank;
	MPI_Comm_size(comm, &p);
	MPI_Comm_rank(comm, &rank);

    int d = 33;
    uint256_t mask = (uint256_t) 1 << d;

    // 0xe7e9033363B988d46fEa4cA7d80ecFc1215eD436
    uint256_t address = ((uint256_t) 0xe7e9033363B988d4 << 96) | ((uint256_t) 0x6fEa4cA7d80ecFc1 << 32) | ((uint256_t) 0x215eD436);
    // std::cout << address << std::endl;

    uint256_t load = mask / (uint256_t) p;
    uint256_t leftOff = 9400000;
    uint256_t nonce = rank * load;
    uint256_t startNonce = nonce;
    nonce += leftOff;
    uint256_t endNonce;
    if (rank == p-1) {
        endNonce = mask;
    } else {
        endNonce = nonce + load;
    }
    std::cout << "Proc " << rank << ": " << nonce << "-" << endNonce << std::endl;

    while (nonce < endNonce) {
        if (rank == 0) std::cout << "Work: " << nonce - startNonce << std::endl;
    
        std::uint8_t data[64] = {0};
        for (size_t i=0; i<32; i++) {
            int shift = (32-i-1)*8;
            data[i] = nonce >> shift;
        }
        for (size_t i=32; i<64; i++) {
            int shift = (64-i-1)*8;
            data[i] = address >> shift;
        }
        // printArr(data, 64, "data");
        
        std::uint8_t hash[Keccak256::HASH_LEN];
        Keccak256::getHash(data, 64, hash);
        // printArr(hash, 32, "hash");

        uint256_t hashInt = 0;
        for (int i=31; i>=0; i--) {
            int shift = (32-i-1)*8;
            hashInt = hashInt | ((uint256_t) hash[i] << shift);
        }
        // std::cout << hashInt << std::endl;
        uint256_t val = hashInt % mask;
        // std::cout << val << std::endl;

        if (val == 0) {
            printArr(data, 64, "data");
            printArr(hash, 32, "hash");
            std::cout << val << std::endl;
            MPI_Abort(comm, MPI_SUCCESS);
            break;
        }

        nonce++;
    }


	MPI_Finalize();
	return 0;
}

/*
mpicxx -o bin/mine *.cpp
mpirun -np 4 bin/mine
*/