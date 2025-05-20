#ifndef BITVECTOR_AMORTIZED_STATIC
#define BITVECTOR_AMORTIZED_STATIC

#include "bitvector_amortized/basics.hpp"

namespace amo {

    class DynamicBV;  // Forward declaration

    class StaticBV {
        public:
            uint64_t size;      // number of bits
            uint64_t ones;      // number of 1s
            uint64_t* data;     // the bits
            uint64_t* S;        // superblocks
            uint16_t* B;        // blocks

            // empty constructor
            StaticBV();
            // constructor with data
            StaticBV(uint64_t* input_data, uint64_t n);
            // Copy constructor
            StaticBV(const StaticBV& other);
            // Move constructor
            StaticBV(StaticBV&& other) noexcept;

            // Destructor
            ~StaticBV();

            // Assignment operator
            StaticBV& operator=(StaticBV&& other) noexcept;

            // Preprocesses for rank, with parameter K
            void staticPreprocess();

            // Writes B's data to file, which must be opened for writing
            uint64_t serialize(std::ostream& out);
            // Loads staticBV's data from file, which must be opened for reading
            // size is the number of bits
            static StaticBV* load(std::istream& in, uint64_t size);

            // Returns pointer to the bits (data)
            uint64_t* bits() const;
            // Returns the size of the bitvector in words of w bits
            uint64_t space() const;
            // Returns the length (in bits)
            uint64_t length() const;
            // Returns the number of ones
            uint64_t getOnes() const;

            // Returns the value of the bit at position i
            uint access_(uint64_t i) const;
            // Computes rank(i), zero-based, assumes i is right
            uint64_t rank_(uint64_t i);
            // Computes select_1(j), zero-based, assumes j is right
            uint64_t select1_(uint64_t j);
            // Computes select_0(j), zero-based, assumes j is right
            uint64_t select0_(uint64_t j);

            // Read bits [i..i+l-1] onto D[j..], assumes it is right
            void read(uint64_t i, uint64_t l, uint64_t* D, uint64_t j);

            DynamicBV* StaticBV::splitFrom (uint64_t *data, uint64_t n, uint64_t ones, uint64_t i);
            DynamicBV* split (uint64_t i);
    };
}

#endif // BITVECTOR_AMORTIZED_STATIC
