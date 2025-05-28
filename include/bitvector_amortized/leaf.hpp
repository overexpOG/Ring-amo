#ifndef BITVECTOR_AMORTIZED_LEAF
#define BITVECTOR_AMORTIZED_LEAF

#include "bitvector_amortized/basics.hpp"

namespace amo {
    
    class DynamicBV;  // Forward declaration

    class LeafBV {
        public:
            uint64_t size;
            uint64_t ones;
            uint64_t* data;

            // empty constructor
            LeafBV();
            // constructor with data
            LeafBV(const uint64_t* inputData, uint n);
            // Copy constructor
            LeafBV(const LeafBV& other);
            // Move constructor
            LeafBV(LeafBV&& other) noexcept;

            // Destructor
            ~LeafBV();

            // Assignment operator
            LeafBV& operator=(LeafBV&& other) noexcept;

            // Writes B's data to file, which must be opened for writing
            uint64_t serialize(std::ostream &out) const;
            // Loads staticBV's data from file, which must be opened for reading
            // size is the number of bits
            static LeafBV* load(std::istream& in, uint size);

            // Returns the size of the bitvector in words of w bits
            uint64_t space() const;
            // Returns the length (in bits)
            uint64_t length() const;
            // Returns the number of ones
            uint64_t getOnes() const;
            // Return the type
            const char* getType() const;

            // Returns the value of the bit at position i
            uint access_(uint i) const;
            // Write bit i, returns difference in ones (+1, 0, -1)
            int write_(uint i, uint v);
            // Insert bit v at position i
            void insert_(uint i, uint v);
            // Delete bit at position i, returns difference in ones
            int remove_(uint i);
            // Computes rank(i), zero-based, assumes i is right
            uint rank_(uint i) const;
            // Computes select_1(j), zero-based, assumes j is right
            uint select1_(uint j) const;
            // Computes select_0(j), zero-based, assumes j is right
            uint select0_(uint j) const;

            // Read bits [i..i+l-1] into D starting at D[j]
            void read(uint i, uint l, uint64_t* D, uint64_t j) const;

            DynamicBV* splitLeaf();
    };
}

#endif // BITVECTOR_AMORTIZED_LEAF
