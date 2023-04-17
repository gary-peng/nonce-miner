#include <mpi.h>
#include <cstdio>
#include <iostream>
#include "Keccak256.hpp"
#include "Uint256.hpp"

#define DIFF 42

void printArr(uint8_t* arr, size_t size, std::string label) {
    std::cout << label + ": ";
    for (size_t i=0; i<size; i++) {
        printf("%02x", arr[i]);
    }
    printf("\n");
}

void printVal(uint32_t arr[Uint256::NUM_WORDS], std::string label) {
    std::cout << label + ": ";
    for (size_t i=0; i<Uint256::NUM_WORDS; i++) {
        printf("%08x", arr[i]);
    }
    printf("\n");
}

bool check(uint8_t hash[Keccak256::HASH_LEN], Uint256 &mask)
{
    Uint256 hashVal = Uint256(hash);
    for (size_t i=0; i<Uint256::NUM_WORDS; i++) {
        hashVal.value[i] &= mask.value[i];
    }

    if (hashVal == Uint256::ZERO) {
        return true;
    }
    return false;
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
	MPI_Comm comm = MPI_COMM_WORLD;
	int p, rank;
	MPI_Comm_size(comm, &p);
	MPI_Comm_rank(comm, &rank);

    Uint256 stepSize = Uint256(); 
    for (int i=0; i<p; i++) {
        stepSize.add(Uint256::ONE);
    }
    Uint256 leftOff = Uint256("000000000000000000000000000000000000000000000000000000ad3c98f780");

    Uint256 address = Uint256("000000000000000000000000e7e9033363B988d46fEa4cA7d80ecFc1215eD436");
    uint8_t addressArr[32];
    address.getBigEndianBytes(addressArr);
    uint8_t data[64] = {0};  
    for (size_t i=0; i<32; i++) {
        data[32+i] = addressArr[i];
    }
    uint8_t hash[Keccak256::HASH_LEN];

    Uint256 nonce = Uint256(); 
    for (int i=0; i<rank; i++) {
        nonce.add(Uint256::ONE);
    }
    nonce.add(leftOff);
    uint8_t nonceArr[32];   
    nonce.getBigEndianBytes(nonceArr);  

    Uint256 mask = Uint256(Uint256::ONE);
    for (size_t i=0; i<DIFF; i++) {
        mask.shiftLeft1();
    }
    mask.subtract(Uint256::ONE);

    if (rank == 0) {
        std::cout << "================================================================================" << std::endl;
        std::cout << "difficulty: " << std::to_string(DIFF) << std::endl;
        printArr(data, 64, "data");
        printf("step size: %d\n", stepSize.value[0]);
        std::cout << "================================================================================" << std::endl;
    }
    printArr(nonceArr, 32, std::to_string(rank) + " starting nonce");

    double prevTime = MPI_Wtime();
    while (true) {
        nonce.getBigEndianBytes(nonceArr);
        for (size_t i=0; i<32; i++) {
            data[i] = nonceArr[i];
        }

        if (rank == 0 && MPI_Wtime()-prevTime >= 1) {
            printArr(nonceArr, 32, std::to_string(rank) + " nonce");
            prevTime = MPI_Wtime();
        }

        Keccak256::getHash(data, 64, hash);

        if (check(hash, mask)) {
            std::cout << "================================================================================" << std::endl;
            printArr(data, 64, "DATA");
            printArr(hash, Keccak256::HASH_LEN, "HASH");
            printArr(nonceArr, 32, std::to_string(rank) + " FOUND NONCE");
            std::cout << "================================================================================" << std::endl;
            break;
        }

        nonce.add(stepSize);
    }

    MPI_Abort(comm, MPI_SUCCESS);
	MPI_Finalize();
	return 0;
}

/*
mpicxx -o bin/mine *.cpp
mpirun -np 120 bin/mine
mpicxx -o bin/mine *.cpp -pthread

42: 000000000000000000000000000000000000000000000000000000ad3c98f780
41: 239700000   0000000000000000000000000000000000000000000000000000000039262080
40: 563000000   00000000000000000000000000000000000000000000000000000000863acb00
*/