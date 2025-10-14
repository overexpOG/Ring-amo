#ifndef BITVECTOR_AMORTIZED_HYBRID
#define BITVECTOR_AMORTIZED_HYBRID

#include "bitvector_amortized/basics.hpp"
#include <variant>

namespace amo {
    class StaticBV;
    class LeafBV;
    class DynamicBV;

    class HybridBV {
        public:
            std::variant<StaticBV*, LeafBV*, DynamicBV*> bv;

            // Empty constructor
            HybridBV();
            // Constructor with data
            HybridBV(uint64_t* data, uint64_t n);
            // Copy constructor
            HybridBV(const HybridBV& other);
            // Move constructor
            HybridBV(HybridBV&& other) noexcept;

            // Destructor
            ~HybridBV();

            // Copy assignment
            HybridBV& operator=(const HybridBV& other);
            // Move assignment
            HybridBV& operator=(HybridBV&& other) noexcept;

            // Public methods
            uint64_t* collect(uint64_t len);
            void flatten(int64_t* delta);
            void read(uint64_t i, uint64_t l, uint64_t* D, uint64_t j); // internal read
            static DynamicBV* splitFrom (uint64_t *data, uint64_t n, uint64_t ones, uint64_t i);
            void balance(uint64_t i, int64_t* delta);
            uint64_t serialize(std::ostream& out);
            uint64_t serialize_(std::ostream& out, uint64_t n);
            void load(std::istream& in);
            void load_(std::istream& in, uint64_t n);
            uint64_t bit_size() const;
            uint64_t leaves() const;
            uint64_t length() const;
            uint64_t getOnes() const;
            const char* getType() const;
            int write_(uint64_t i, uint v);
            void irecompute(uint64_t i);
            void rrecompute(uint64_t i, uint64_t l);
            void insert_(uint64_t i, uint v, uint* recalc);
            void hybridInsert_(uint64_t i, uint v);
            int remove_(uint64_t i, uint* recalc);
            int hybridRemove_(uint64_t i);
            void recompute(uint64_t i, int64_t delta);
            bool mustFlatten(uint64_t n);
            uint access_(uint64_t i, int64_t* delta, uint64_t n);
            uint hybridAccess_(uint64_t i);
            void read(uint64_t i, uint64_t l, uint64_t* D, uint64_t j, uint* recomp, uint64_t n); // external read
            void hybridRead(uint64_t i, uint64_t l, uint64_t* D, uint64_t j);
            uint64_t rank_(uint64_t i, int64_t* delta, uint64_t n);
            uint64_t hybridRank_(uint64_t i);
            uint64_t select1_(uint64_t j, int64_t* delta, uint64_t n);
            uint64_t select0_(uint64_t j, int64_t* delta, uint64_t n);
            uint64_t hybridSelect1_(uint64_t j);
            uint64_t hybridSelect0_(uint64_t j);
            int64_t next1(uint64_t i, int64_t *delta, uint64_t n);
            int64_t next0(uint64_t i, int64_t *delta, uint64_t n);
            int64_t hybridNext1(uint64_t i);
            int64_t hybridNext0(uint64_t i);

            // Convenience methods (wrappers)
            uint64_t at(uint64_t i);
            uint64_t rank(uint64_t i, bool b = 1);
            uint64_t select1(uint64_t i);
            uint64_t select0(uint64_t i);
            uint64_t select(uint64_t i, bool b);
            uint64_t size();
            void insert(uint64_t i, uint v);
            void insert0(uint64_t i);
            void insert1(uint64_t i);
            void push_back(uint i);
            int remove(uint64_t i);
            int set(uint64_t i, bool b = true);

        private:
            // Helper method for destructor and assignment operators
            void deleteBV();
        };
}

#endif // BITVECTOR_AMORTIZED_HYBRID
