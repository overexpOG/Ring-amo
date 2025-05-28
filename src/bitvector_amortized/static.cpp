#include "bitvector_amortized/dynamic.hpp"
#include "bitvector_amortized/hybrid.hpp"
#include "bitvector_amortized/leaf.hpp"
#include "bitvector_amortized/static.hpp"

namespace amo {
    // constructor por defecto
    StaticBV::StaticBV() {}

    // Constructor a partir de datos existentes (data se transfiere, no copia)
    StaticBV::StaticBV(uint64_t* input_data, uint64_t n) : size(n) {
        size = n;
        if (n == 0) {
            data = nullptr;
        } else {
            data = input_data;
        }
        S = nullptr;
        B = nullptr;
        staticPreprocess();
    }

    // constructor por copia
    StaticBV::StaticBV(const StaticBV& other) : size(other.size), ones(other.ones) {
        // Copiar data (si existe)
        if (size == 0) {
            data = nullptr;
        } else {
            uint64_t nb = (size + 63) / 64;
            data = new uint64_t[nb];
            memcpy(data, other.data, nb * sizeof(uint64_t));
        }
        // Copiar S (si existe)
        if (other.S != nullptr) {
            uint64_t s_len = (size + (1ULL << w16) - 1) / (1ULL << w16);
            S = new uint64_t[s_len];
            memcpy(S, other.S, s_len * sizeof(uint64_t));
        } else {
            S = nullptr;
        }
        // Copiar B (si existe)
        if (other.B != nullptr) {
            uint64_t b_len = (size + K * w - 1) / (K * w);
            B = new uint16_t[b_len];
            memcpy(B, other.B, b_len * sizeof(uint16_t));
        } else {
            B = nullptr;
        }
    }

    // constructor por movimiento
    StaticBV::StaticBV(StaticBV&& other) noexcept {
        size = other.size;
        ones = other.ones;
        data = other.data;
        S = other.S;
        B = other.B;

        other.data = nullptr;
        other.S = nullptr;
        other.B = nullptr;
        other.size = 0;
        other.ones = 0;
    }

    // Destructor
    StaticBV::~StaticBV() {
        delete[] data;
        delete[] S;
        delete[] B;
    }

    // operador de copia
    StaticBV& StaticBV::operator=(StaticBV&& other) noexcept {
        if (this != &other) {
            delete[] data;
            delete[] S;
            delete[] B;

            size = other.size;
            ones = other.ones;
            data = other.data;
            S = other.S;
            B = other.B;

            other.data = nullptr;
            other.S = nullptr;
            other.B = nullptr;
            other.size = 0;
            other.ones = 0;
        }
        return *this;
    }

    // preprocesses for rank, with parameter K
    void StaticBV::staticPreprocess () { 
        uint64_t i,n;
        uint64_t sacc,acc;
        n = size;
        if (n == 0) return;
        B = new uint16_t[(n + K*w - 1) / (K*w)];
        S = new uint64_t[(n + (1 << w16) - 1) / (1 << w16)];
        sacc = acc = 0;
        i = 0;
        while (i<(n+w-1)/w) { 
            uint64_t top = std::min((n+w-1)/w,i+(1<<w16)/w);
            sacc += acc; acc = 0;
            S[(i*w) >> w16] = sacc;
            while (i<top) { 
                if (i%K == 0) {
                    B[i/K] = static_cast<uint16_t>(acc);
                }
                acc += popcount(data[i]);
                i++;
            }
        }
        ones = rank_(n-1);
    }

    // writes B's data to file, which must be opened for writing 
    uint64_t StaticBV::serialize (std::ostream &out) { 
        if (size != 0) {
            return myfwrite(data,sizeof(uint64_t),(size+w-1)/w, out);
        }

        return 0;
    }

    // loads staticBV's data from file, which must be opened for reading
    // size is the number of bits
    StaticBV* StaticBV::load (std::istream& in, uint64_t size) {
        uint64_t *data = new uint64_t[(size + w - 1) / w];
        myfread (data,sizeof(uint64_t),(size + w - 1)/w,in);
        
        StaticBV *stat = new StaticBV(data, size);
        return stat;
    }

    // Devuelve puntero a los bits
    uint64_t* StaticBV::bits() const {
        return data;
    }

    // Devuelve el tamaño del bitvector en palabras de w bits
    uint64_t StaticBV::space() const {
        if (!this) return 0;
        uint64_t space = sizeof(StaticBV) * 8 / w;
        if (data) space += (size + w - 1) / w;
        if (B) space += ((size + K * w - 1) / (K * w)) / (w / w16);
        if (S) space += (size + (1ULL << w16) - 1) / (1ULL << w16);
        return space;
    }

    // Devuelve el largo (en bits)
    uint64_t StaticBV::length() const {
        return size;
    }

    // Devuelve la cantidad de unos
    uint64_t StaticBV::getOnes() const {
        return ones;
    }

    const char* StaticBV::getType() const { 
        return "StaticBV";
    }

    // Devuelve el valor del bit en la posición i
    uint StaticBV::access_(uint64_t i) const {
        return (data[i / w] >> (i % w)) & 1;
    }

    // read bits [i..i+l-1] onto D[j..], assumes it is right  
    void StaticBV::read (uint64_t i, uint64_t l, uint64_t * D, uint64_t j) { 
        copyBits(D,j,data,i,l);
    }

    // computes rank(i), zero-based, assumes i is right
    uint64_t StaticBV::rank_(uint64_t i) {
        uint64_t b,sb;
        uint64_t rank;
        sb = i/(K*w);
        rank = S[i>>w16] + B[sb];
        sb *= K;
        for (b=sb;b<i/w;b++) {
            rank += popcount(data[b]);
        }
        return rank + popcount(data[b] & (((uint64_t)~0) >> (w-1-(i%w))));
    }

    // computes select_1(j), zero-based, assumes j is right
    uint64_t StaticBV::select1_(uint64_t j) {
        int64_t i,d,b;
        uint p;
        uint64_t word,s,m,n;
        n = size;
        s = (n+(1<<w16)-1)/(1<<w16);
        // interpolation: guess + exponential search
        i = ((uint64_t)(j * (n / (float)ones))) >> w16;
        if (i == s) i--;
        if (S[i] < j) { 
            d = 1;
            while ((i+d < s) && (S[i+d] < j)) { 
                i += d; d <<= 1; 
            }
            // now d is the top of the range
            d = std::min(static_cast<int64_t>(s),i+d); // (uint64_t, int64_t)
            while (i+1<d) { 
                m = (i+d)>>1;
                if (S[m] < j) {
                    i = m;
                } else {
                    d = m;
                }
            }
        } else { 
            d = 1;
            while ((i-d >= 0) && (S[i-d] >= j)) { 
                i -= d; d <<= 1; 
            }
            // now d is the bottom of the range
            d = std::max(static_cast<int64_t>(0LL),i-d);
            while (d+1<i) { 
                m = (i+d)>>1;
                if (S[m] < j) {
                    d = m;
                } else {
                    i = m;
                }
            }
            i--;
        }
        // now the same inside the superblock
        // what remains to be found inside the superblock
        j -= S[i];
        p = i < s-1 ? S[i+1]-S[i] : ones-S[i];
        b = (i << w16) / (w*K);
        s = std::min(static_cast<uint64_t>(b+(1<<w16)/(w*K)),(n+w*K-1)/(w*K));
        i = b + ((j * (s-b)*(w*K) / (float)p)) / (w*K);
        if (i == s) i--;
        if (B[i] < j) { 
            d = 1;
            while ((i+d < s) && (B[i+d] < j)) { 
                i += d; d <<= 1; 
            }
            // now d is the top of the range
            d = std::min(static_cast<int64_t>(s),i+d); // (uint64_t, int64_t)
            while (i+1<d) { 
                m = (i+d)>>1;
                if (B[m] < j) {
                    i = m;
                } else {
                    d = m;
                }
            }
        } else { 
            d = 1;
            while ((i-d >= b) && (B[i-d] >= j)) { 
                i -= d; d <<= 1; 
            }
            // now d is the bottom of the range
            d = std::max(b,i-d);
            while (d+1<i) { 
                m = (i+d)>>1;
                if (B[m] < j) {
                    d = m;
                    } else {
                        i = m;
                    }
            }
            i--;
        }
        // now it's confined to K blocks
        j -= B[i];
        i *= K;
        while ((i+1)*w < n) { 
            p = popcount(data[i]);
            if (p >= j) break;
            j -= p;
            i++;
        }
        word = data[i];
        i *= w;
        while (1) { 
            j -= word & 1;
            if (j == 0) return i;
            word >>= 1;
            i++;
        }
    };

    // computes select_1(j), zero-based, assumes j is right
    uint64_t StaticBV::select0_(uint64_t j) {
        int64_t i,d,b;
        uint p;
        uint64_t word,s,m,n;
        n = size;
        s = (n+(1<<w16)-1)/(1<<w16);
        
        // Define función que calcula ceros acumulados hasta el i-esimo superbloque (S)
        auto SZeros = [&](int64_t idx) -> uint64_t {
            // zeros = posiciones - unos acumulados
            return (idx * (1ULL << w16)) - S[idx];
        };

        // Cantidad total de ceros
        uint64_t zeros = n - ones;
        // interpolation: guess + exponential search
        i = ((uint64_t)(j * (n / (float)zeros))) >> w16;
        if (i == s) i--;

        if (SZeros(i) < j) { 
            d = 1;
            while ((i+d < s) && (SZeros(i+d) < j)) { 
                i += d; d <<= 1; 
            }
            // now d is the top of the range
            d = std::min(static_cast<int64_t>(s),i+d); // (uint64_t, int64_t)
            while (i+1<d) { 
                m = (i+d)>>1;
                if (SZeros(m) < j) {
                    i = m;
                } else {
                    d = m;
                }
            }
        } else {
            d = 1;
            while ((i-d >= 0) && (SZeros(i-d) >= j)) { 
                i -= d; d <<= 1; 
            }
            // now d is the bottom of the range
            d = std::max(static_cast<int64_t>(0LL),i-d);
            while (d+1<i) { 
                m = (i+d)>>1;
                if (SZeros(m) < j) {
                    d = m;
                } else {
                    i = m;
                }
            }
            i--;
        }
        // now the same inside the superblock
        // what remains to be found inside the superblock

        // Define función que calcula ceros acumulados hasta el i-esimo bloque (B)
        auto BZeros = [&](int64_t idx) -> uint64_t {
            // zeros = posiciones - unos acumulados
            return idx * (w * K) - B[idx];
        };

        j -= SZeros(i);
        p = i < s-1 ? SZeros(i+1)-SZeros(i) : zeros-SZeros(i);
        b = (i << w16) / (w*K);
        s = std::min(static_cast<uint64_t>(b+(1<<w16)/(w*K)),(n+w*K-1)/(w*K));
        i = b + ((j * (s-b)*(w*K) / (float)p)) / (w*K);
        if (i == s) i--;
        if (BZeros(i) < j) { 
            d = 1;
            while ((i+d < s) && (BZeros(i+d) < j)) { 
                i += d; d <<= 1; 
            }
            // now d is the top of the range
            d = std::min(static_cast<int64_t>(s),i+d); // (uint64_t, int64_t)
            while (i+1<d) { 
                m = (i+d)>>1;
                if (BZeros(m) < j) {
                    i = m;
                } else {
                    d = m;
                }
            }
        } else { 
            d = 1;
            while ((i-d >= b) && (BZeros(i-d) >= j)) { 
                i -= d; d <<= 1; 
            }
            // now d is the bottom of the range
            d = std::max(b,i-d);
            while (d+1<i) { 
                m = (i+d)>>1;
                if (BZeros(m) < j) {
                    d = m;
                    } else {
                        i = m;
                    }
            }
            i--;
        }
        // now it's confined to K blocks
        j -= BZeros(i);
        i *= K;
        while ((i+1)*w < n) { 
            p = w - popcount(data[i]);
            if (p >= j) break;
            j -= p;
            i++;
        }
        word = data[i];
        i *= w;
        while (1) { 
            j -= 1 - (word & 1);
            if (j == 0) return i;
            word >>= 1;
            i++;
        }
    };

    //eliminar despues StaticBV
    DynamicBV* StaticBV::split (uint64_t i) {
        return HybridBV::splitFrom(data,length(),getOnes(),i);
    }
}