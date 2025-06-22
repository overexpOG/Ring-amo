#include <variant>
#include "bitvector_amortized/leaf.hpp"
#include "bitvector_amortized/dynamic.hpp"
#include "bitvector_amortized/hybrid.hpp"

namespace amo {
    // constructor por defecto
    DynamicBV::DynamicBV() {}

    // constructor por copia
    DynamicBV::DynamicBV(const DynamicBV& other) {
        size = other.size;
        ones = other.ones;
        leaves = other.leaves;
        accesses = other.accesses;

        // Copia profunda de los hijos (usando el constructor por copia de HybridBV)
        left = new HybridBV(*other.left);
        right = new HybridBV(*other.right);
    }

    // Constructor por movimiento
    DynamicBV::DynamicBV(DynamicBV&& other) noexcept
        : size(other.size), ones(other.ones), leaves(other.leaves), accesses(other.accesses),
        left(other.left), right(other.right)
    {
        other.left = nullptr;   // dejamos el objeto en estado válido (sin punteros)
        other.right = nullptr;
        other.size = 0;
        other.ones = 0;
        other.leaves = 0;
        other.accesses = 0;
    }

    // Operador de asignación por movimiento
    DynamicBV& DynamicBV::operator=(DynamicBV&& other) noexcept {
        if (this != &other) {
            delete left;    // liberamos la memoria actual
            delete right;

            size = other.size;
            ones = other.ones;
            leaves = other.leaves;
            accesses = other.accesses;

            left = other.left;   // transferimos propiedad
            right = other.right;

            other.left = nullptr;    // dejamos other limpio
            other.right = nullptr;
            other.size = 0;
            other.ones = 0;
            other.leaves = 0;
            other.accesses = 0;
        }
        return *this;
    }

    // merge the two leaf children of B into a leaf
    // returns a leafBV and destroys B
    LeafBV* DynamicBV::mergeLeaves () { // eliminar dynamic
        LeafBV *LB1,*LB2;

        auto leftLeaf = std::get_if<LeafBV*>(&left->bv);
        auto rightLeaf = std::get_if<LeafBV*>(&right->bv);
        if (!leftLeaf || !rightLeaf) {
            throw std::runtime_error("El variant no contiene un LeafBV en left o right");
        }
        LB1 = *leftLeaf;
        LB2 = *rightLeaf;
        copyBits(LB1->data,LB1->size,LB2->data,0,LB2->size);
        LB1->size += LB2->size;
        LB1->ones += LB2->ones;
        delete LB2;

        return LB1;
    }

    // transfers bits from the right to the left leaves children of B
    // tells if it transfered something
    int DynamicBV::transferLeft () { 
        LeafBV *LB1,*LB2;
        uint i,trf,ones,words;
        uint64_t *segment;

        auto leftLeaf = std::get_if<LeafBV*>(&left->bv);
        auto rightLeaf = std::get_if<LeafBV*>(&right->bv);
        if (!leftLeaf || !rightLeaf) {
            throw std::runtime_error("El variant no contiene un LeafBV en left o right");
        }

        LB1 = *leftLeaf;
        LB2 = *rightLeaf;
        trf = (LB2->size-LB1->size+1)/2;
        if (trf < leafMaxSize() * w * TrfFactor) return 0;
        copyBits(LB1->data,LB1->size,LB2->data,0,trf);
        LB1->size += trf;
        LB2->size -= trf;
        segment = new uint64_t[leafMaxSize()];
        copyBits(segment,0,LB2->data,trf,LB2->size);
        words = trf / w;
        ones = 0;
        for (i=0;i<words;i++) ones += popcount(LB2->data[i]);
        if (trf % w) ones += popcount(LB2->data[words] & 
                    (((uint64_t)1) << (trf % w)) - 1);
        LB1->ones += ones;
        LB2->ones -= ones;
        memcpy(LB2->data,segment,(LB2->size+7)/8);
        delete[] segment;
        return 1;
    }

    // transfers bits from the left to the right leaves children of B
    // tells if it transfered something
    int DynamicBV::transferRight () { 
        LeafBV *LB1,*LB2;
        uint i,trf,ones,words;
        uint64_t *segment;

        auto leftLeaf = std::get_if<LeafBV*>(&left->bv);
        auto rightLeaf = std::get_if<LeafBV*>(&right->bv);
        if (!leftLeaf || !rightLeaf) {
            throw std::runtime_error("El variant no contiene un LeafBV en left o right");
        }

        LB1 = *leftLeaf;
        LB2 = *rightLeaf;
        trf = (LB1->size-LB2->size+1)/2;
        if (trf < leafMaxSize() * w * TrfFactor) return 0;
        segment = new uint64_t[leafMaxSize()];
        memcpy(segment,LB2->data,(LB2->size+7)/8);
        copyBits(LB2->data,0,LB1->data,LB1->size-trf,trf);
        words = trf / w;
        ones = 0;
        for (i=0;i<words;i++) {
            ones += popcount(LB2->data[i]);
        }
        if (trf % w) {
            ones += popcount(LB2->data[words] & (((uint64_t)1) << (trf % w)) - 1);
        }
        LB1->ones -= ones;
        LB2->ones += ones;
        copyBits(LB2->data,trf,segment,0,LB2->size);
        LB1->size -= trf;
        LB2->size += trf;
        delete[] segment;
        return 1;
    }

    uint64_t DynamicBV::getLeaves() const {
        return leaves;
    }

    uint64_t DynamicBV::bit_size() const {
        return (sizeof(DynamicBV)*8+w-1)/w + left->bit_size() + right->bit_size();
    }

    // Devuelve el largo (en bits)
    uint64_t DynamicBV::length() const {
        return size;
    }

    // Devuelve la cantidad de unos
    uint64_t DynamicBV::getOnes() const {
        return ones;
    }

    const char* DynamicBV::getType() const { 
        return "DynamicBV";
    }
}