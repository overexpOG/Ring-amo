#ifndef BITVECTOR_AMORTIZED_DYNAMIC
#define BITVECTOR_AMORTIZED_DYNAMIC

#include "bitvector_amortized/basics.hpp"

namespace amo {
    class HybridBV;
    class LeafBV;

    class DynamicBV {
        public:
            uint64_t size;          // the size of the bitvector
            uint64_t ones;          // the number of ones
            uint64_t leaves;        // the number of leaves
            uint64_t accesses;      // since last update
            HybridBV* left;         // Pointer to the left child (subtree)
            HybridBV* right;        // Pointer to the right child (subtree)

            // Default constructor
            DynamicBV();
            // Copy constructor
            DynamicBV(const DynamicBV& other);
            // Move constructor
            DynamicBV(DynamicBV&& other) noexcept;

            // Destructor
            ~DynamicBV() = default;

            // Move assignment operator
            DynamicBV& operator=(DynamicBV&& other) noexcept;


            // merge the two leaf children of B into a leaf
            // returns a leafBV and destroys B
            LeafBV* mergeLeaves();
            // transfers bits from the right to the left leaves children of B
            // tells if it transferred something
            int transferLeft();
            // transfers bits from the left to the right leaves children of B
            // tells if it transferred something
            int transferRight();

            // return the number of leaves
            uint64_t getLeaves() const;
            // Returns the size of the bitvector in words of w bits
            uint64_t space() const;
            // Returns the length (in bits)
            uint64_t length() const;
            // Returns the number of ones
            uint64_t getOnes() const;
    };
}

#endif // BITVECTOR_AMORTIZED_DYNAMIC
