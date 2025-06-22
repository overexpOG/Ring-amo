#include "bitvector_amortized/leaf.hpp"
#include "bitvector_amortized/dynamic.hpp"
#include "bitvector_amortized/hybrid.hpp"

namespace amo {
    // Constructor hoja vacía
    LeafBV::LeafBV() : size(0), ones(0) {
        data = new uint64_t[MaxBlockWords];
    }

    // Constructor con datos externos (copia data)
    LeafBV::LeafBV(const uint64_t* inputData, uint n) : size(n), ones(0) {
        data = new uint64_t[MaxBlockWords];

        // Copiar datos, solo los bytes necesarios
        size_t bytesToCopy = (n + 7) / 8;
        memcpy(data, inputData, bytesToCopy);

        uint nb = (n + w - 1) / w;
        if (n % w) {
            data[nb - 1] &= (((uint64_t)1) << (n % w)) - 1;
        }
        for (uint i = 0; i < nb; i++) {
            ones += popcount(data[i]);
        }
    }

    // constructor por copia
    LeafBV::LeafBV(const LeafBV& other) : size(other.size), ones(other.ones) {
        data = new uint64_t[MaxBlockWords];
        memcpy(data, other.data, MaxBlockWords * sizeof(uint64_t));
    }

    // Constructor por movimiento
    LeafBV::LeafBV(LeafBV&& other) noexcept
    : size(other.size), ones(other.ones), data(other.data)
    {
        other.data = nullptr;  // transferimos la propiedad del puntero
        other.size = 0;
        other.ones = 0;
    }

    // Destructor
    LeafBV::~LeafBV() {
        delete[] data;
    }

    // Operador de asignación por movimiento
    LeafBV& LeafBV::operator=(LeafBV&& other) noexcept {
        if (this != &other) {
            delete[] data;       // liberamos la memoria actual

            data = other.data;   // transferimos punteros y datos
            size = other.size;
            ones = other.ones;

            other.data = nullptr;  // dejamos el objeto en estado válido
            other.size = 0;
            other.ones = 0;
        }
        return *this;
    }

    // Guardar a archivo (binario)
    uint64_t LeafBV::serialize(std::ostream &out) const {
        if (this->size != 0) {
            return myfwrite(this->data, sizeof(uint64_t), (this->size+w-1)/w, out);
        }

        return 0;
    }

    // Cargar desde archivo (binario)
    LeafBV* LeafBV::load(std::istream& in, uint size) {
        uint64_t *data = new uint64_t[MaxBlockWords];
        myfread (data,sizeof(uint64_t),(size+w-1)/w,in);

        LeafBV *leaf = new LeafBV(data, size);
        delete[] data;
        return leaf;
    }

    // Espacio usado en palabras w-bit
    uint64_t LeafBV::bit_size() const {
        return (sizeof(*this) + sizeof(uint64_t) - 1) / sizeof(uint64_t) + MaxBlockWords;
    }

    // Longitud en bits
    uint64_t LeafBV::length() const {
        return size;
    }

    // Cantidad de unos
    uint64_t LeafBV::getOnes() const {
        return ones;
    }

    const char* LeafBV::getType() const { 
        return "LeafBV";
    }

    // Leer bit i
    uint LeafBV::access_(uint i) const {
        return (data[i/w] >> (i%w)) & 1;
    }

    // Escribir bit i, retorna diferencia en unos (+1, 0, -1)
    int LeafBV::write_(uint i, uint v) {
        uint64_t one = ((uint64_t)1) << (i%w);
        if (v) {
            if (!(data[i/w] & one)) {
                data[i/w] |= one;
                ones++;
                return 1;
            }
        } else {
            if (data[i/w] & one) {
                data[i/w] &= ~one;
                ones--;
                return -1;
            }
        }
        return 0;
    }

    // Insertar bit v en posición i
    void LeafBV::insert_(uint i, uint v) {
        uint nb = ++size/w;
        uint ib = i/w;
        int b;

        for (b=nb;b>ib;b--) {
            data[b] = (data[b] << 1) | (data[b-1] >> (w-1));
        }
        if ((i+1)%w) {
            data[ib] =   (data[ib] & ((((uint64_t)1) << (i%w)) - 1)) | (((uint64_t)v) << (i%w)) |
                            ((data[ib] << 1) & (~((uint64_t)0) << ((i+1)%w)));
        } else {
            data[ib] = (data[ib] & ((((uint64_t)1) << (i%w)) - 1)) | (((uint64_t)v) << (i%w));
        }
        ones += v;
    }

    // Borrar bit en posición i, retorna diferencia en unos
    int LeafBV::remove_(uint i) {
        uint nb = size--/w;
        uint ib = i/w;
        int b;
        int v = (data[ib] >> (i%w)) & 1;

        data[ib] =   (data[ib] & ((((uint64_t)1) << (i%w)) - 1)) | ((data[ib] >> 1) & 
                        (~((uint64_t)0) << (i%w)));
        for (b=ib+1;b<=nb;b++) {
            data[b-1] |= data[b] << (w-1);
            data[b] >>= 1;
        }
        ones -= v;
        return -v;
    }

    // rank1 hasta posición i (inclusive)
    uint LeafBV::rank_(uint i) const {
        int p,ib;
        uint ones = 0;
        ib = i/w;
        for (p=0;p<ib;p++) ones += popcount(data[p]);
        if (i%w) ones += popcount(data[p] & ((((uint64_t)1)<<(i%w))-1));
        return ones;
    }

    // select1: posición del j-ésimo 1 (1-based)
    uint LeafBV::select1_(uint j) const {
        uint p,i,pc;
        uint64_t word;
        uint ones = 0;
        p = 0;
        while (1) { 
            word = data[p];
            pc = popcount(word);
            if (ones+pc >= j) break;
            ones += pc; 
            p++;
        }
        i = p*w;
        while (1) { 
            ones += word & 1; 
            if (ones == j) return i;
            word >>= 1; 
            i++; 
        }
    }

    // select0: posición del j-ésimo 0 (0-based)
    uint LeafBV::select0_(uint j) const {
        uint p,i,pc;
        uint64_t word;
        uint zeros = 0;
        p = 0;
        while (1) { 
            word = data[p];
            pc = w - popcount(word);
            if (zeros+pc >= j) break;
            zeros += pc; 
            p++;
        }
        i = p*w;
        while (1) { 
            zeros += 1 - (word & 1);
            if (zeros == j) return i;
            word >>= 1; 
            i++; 
        }
    }

    // trick for lowest 1 in a 64-bit word
    static int decode[64] = {
        0, 1,56, 2,57,49,28, 3,61,58,42,50,38,29,17, 4,
        62,47,59,36,45,43,51,22,53,39,33,30,24,18,12, 5,
        63,55,48,27,60,41,37,16,46,35,44,21,52,32,23,11,
        54,26,40,15,34,20,31,10,25,14,19, 9,13, 8, 7, 6 };

    // computes next_1(B,i), zero-based and including i
    // returns -1 if no answer
    int LeafBV::next1(uint i) { 
        uint p;
        uint64_t word;
        p = i/w;
        word = data[p] & ((~(uint64_t)0)<<(i%w));
        while ((++p*w <= size) && !word) {
            word = data[p];
        }
        if (p*w > size) {
            word &= (((uint64_t)1) << (size % w)) - 1;
        }
        if (!word) {
            return -1;
        }
        return (p-1)*w + decode[(0x03f79d71b4ca8b09 * (word & -word))>>58];
    }

    // computes next_0(B,i), zero-based and including i
    // returns -1 if no answer
    int LeafBV::next0(uint i) { 
        uint p;
        uint64_t word;
        p = i/w;
        word = (~data[p]) & ((~(uint64_t)0)<<(i%w));
        while ((++p*w <= size) && word) {
            word = ~data[p];
        }
        if (p*w > size) {
            word &= (((uint64_t)1) << (size % w)) - 1;
        }
        if (!word) {
            return -1;
        }
        return (p-1)*w + decode[(0x03f79d71b4ca8b09 * (word & -word))>>58];
    }

    // Leer bits [i..i+l-1] en D a partir de D[j]
    void LeafBV::read(uint i, uint l, uint64_t* D, uint64_t j) const {
        // Aquí deberías implementar copyBits o similar
        copyBits(D,j,data,i,l);
    }

    // splits a full leaf into two
    // returns a dynamicBV
    DynamicBV* LeafBV::splitLeaf() { //eliminar despues LeafBV
        HybridBV *HB1,*HB2;
        DynamicBV *DB;
        uint bsize;

        // byte size of new left leaf
        bsize = (size/2+7)/8;
        HB1 = new HybridBV(data,bsize*8);
        HB2 = new HybridBV((uint64_t*)(((unsigned char*)data)+bsize),size-bsize*8);
        DB = new DynamicBV();
        DB->size = size;
        DB->ones = ones;
        DB->leaves = 2;
        DB->accesses = 0;
        DB->left = HB1;
        DB->right = HB2;
        return DB;
    }

    std::ostream& operator<<(std::ostream& os, const LeafBV& bv) {
        os << "[LeafBV] size: " << bv.size
        << ", ones: " << bv.ones
        << ", data ptr: " << static_cast<const void*>(bv.data);
        return os;
    }
}