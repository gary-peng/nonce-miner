#include <mpi.h>
#include <cstdio>
#include <iostream>
#include <thread>
#include "Keccak256.hpp"
#include "uint256_t.h"

#define DIFF 33
#define LEFT_OFF 0

void printArr(std::uint8_t* arr, size_t size, std::string label) {
    std::cout << label + ": ";
    for (size_t i=0; i<size; i++) {
        printf("%02x", arr[i]);
    }
    printf("\n");
}

void hash(uint256_t threadStartNonce, uint256_t threadEndNonce, int rank, int threadRank, uint256_t address, uint256_t mask, double startTime) {
    uint256_t nonce = threadStartNonce;

    while (nonce < threadEndNonce) {
        uint256_t work = nonce - threadStartNonce;
        if (rank == 0 && threadRank == 0 && work%50000 == 0) std::cout << "Work: " << nonce - threadStartNonce << std::endl;
    
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
            std::cout << rank << "nonce: " << nonce << std::endl;

            double totalTime = MPI_Wtime()-startTime;
            printf("Time: %.6f\n", totalTime);

            MPI_Abort(MPI_COMM_WORLD, MPI_SUCCESS);
            break;
        }

        nonce++;
    }
}

int main(int argc, char *argv[]) {
    int NUM_THREADS = atoi(argv[1]);
    // std::cout << "num threads: " << NUM_THREADS << std::endl;

    int buf[100], provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided); 
	MPI_Comm comm = MPI_COMM_WORLD;
	int p, rank;
	MPI_Comm_size(comm, &p);
	MPI_Comm_rank(comm, &rank);

    uint256_t mask = (uint256_t) 1 << DIFF;
    // 0xe7e9033363B988d46fEa4cA7d80ecFc1215eD436
    uint256_t address = ((uint256_t) 0xe7e9033363B988d4 << 96) | ((uint256_t) 0x6fEa4cA7d80ecFc1 << 32) | ((uint256_t) 0x215eD436);

    uint256_t searchEnd = ((uint256_t) 1 << 256) - 1;
    uint256_t searchStart = 0;
    uint256_t load = (searchEnd-searchStart) / (uint256_t) p;
    uint256_t startNonce = (rank * load) + searchStart;
    uint256_t endNonce;
    if (rank == p-1) {
        endNonce = searchEnd;
    } else {
        endNonce = startNonce + load;
    }
    std::cout << "Proc " << rank << ": " << startNonce << "-" << endNonce << std::endl;

    double startTime = MPI_Wtime();

    std::thread thrds[NUM_THREADS];
    for (int i=0; i<NUM_THREADS; i++){
        uint256_t threadLoad = load / NUM_THREADS;
        uint256_t threadStartNonce = (threadLoad * i) + startNonce + LEFT_OFF;
        uint256_t threadEndNonce;
        if (i == NUM_THREADS-1) {
            threadEndNonce = endNonce;
        } else {
            threadEndNonce = threadStartNonce + threadLoad;
        }

        // std::cout << "Proc " << rank << "|" << i << ": " << threadStartNonce << "-" << threadEndNonce << std::endl;

        thrds[i] = std::thread(hash, threadStartNonce, threadEndNonce, rank, i, address, mask, startTime);
    }
    for (size_t i=0; i<NUM_THREADS; i++){
        thrds[i].join();
    }

    double totalTime = MPI_Wtime()-startTime;
    printf("Time: %.6f\n", totalTime);

	MPI_Finalize();
	return 0;
}

/*
mpicxx -o bin/mine *.cpp
mpirun -np 4 bin/mine 10
mpicxx -o bin/mine *.cpp -pthread
85577470
*/