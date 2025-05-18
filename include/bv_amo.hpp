#include <variant>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <ostream>

using uint = uint32_t;

float FactorBV = 1.0; // Factor * length reads => rebuild as static
constexpr size_t w = 8 * sizeof(uint64_t);
constexpr int MaxBlockWords = 32;
constexpr double NewFraction = 0.75;
constexpr int w = 64;                           // bits por palabra
constexpr int K = 4;                            // block length is w*K
constexpr size_t w16 = 8 * sizeof(uint16_t);    // superblock length is 2^w16
constexpr float TrfFactor = 0.125;              // TrfFactor * MaxLeafSize to justify transferLeft/Right
constexpr float AlphaFactor = 0.65;             // balance factor 3/5 < . < 1
static constexpr int MinLeavesToBalance = 5;    // min number of leaves to balance the tree
static constexpr float MinFillFactor = 0.3;     // less than this involves rebuild. Must be <= NewFraction/2

uint popcount (uint64_t y) { 
    y -= ((y >> 1) & 0x5555555555555555ull);
    y = (y & 0x3333333333333333ull) + (y >> 2 & 0x3333333333333333ull);
    return ((y + (y >> 4)) & 0xf0f0f0f0f0f0f0full) * 0x101010101010101ull >> 56;
}

void myfread(void* ptr, size_t size, size_t nmemb, std::istream& in) {
    // Leer del stream
    in.read(static_cast<char*>(ptr), size * nmemb);

    // Verificar la cantidad de datos realmente leídos
    if (in.gcount() != static_cast<std::streamsize>(size * nmemb)) {
        std::cerr << "Error: fread of " << nmemb * size << " bytes failed\n";
        std::exit(1);
    }
}

uint64_t myfwrite(void* ptr, size_t size, size_t nmemb, std::ostream& out) {
    // Escribir los datos en el flujo
    out.write(static_cast<char*>(ptr), size * nmemb);

    // Verificar errores de escritura
    if (!out) {
        std::cerr << "Error: write failed\n";
        std::exit(1);
    }

    return size * nmemb;
}

// copies len bits starting at *src + psrc
// to tgt from bit position ptgt
// WARNING: writes some extra bits after target (but not more words)
void copyBits (uint64_t* tgt, uint64_t ptgt, uint64_t* src, uint64_t psrc, uint64_t len) { 
    uint64_t old,mask;
    // easy cases if they are similarly misaligned
    // I didn't align to 8 due to possible endianness issues
    tgt += ptgt/w; ptgt %= w;
    src += psrc/w; psrc %= w;
    mask = (((uint64_t)1) << ptgt)-1;

    if (ptgt == psrc) { 
        if (ptgt != 0) { 
            *tgt = (*tgt & mask) + (*src & ~mask);
            *tgt++; *src++; len -= w-ptgt;
        }
        std::memcpy (tgt,src,((len+w-1)/w)*sizeof(uint64_t));
        return;
    }
    // general case, we first align the source
    // first word from src fits in ptgt
    if (ptgt < psrc) { 
        *tgt = (*tgt & mask) + ((*src++ >> (psrc-ptgt)) & ~mask);
        ptgt += w-psrc;
    } else { 
        *tgt = (*tgt & mask) + ((*src << (ptgt-psrc)) & ~mask);
        // before overflowing to the next word
        if (len <= w-ptgt) return;
        ptgt -= psrc;
        *++tgt = *src++ >> (w-ptgt); 
    }
    if (len <= w-psrc) return;
    len -= w-psrc;
    // now src is aligned, copy all the rest
    mask = (((uint64_t)1) << ptgt)-1;
    old = *tgt & mask;
    // cannot write len >= 0 as it is unsigned
    len += w;
    while (len > w) { 
        *tgt++ = old + (*src << ptgt);
        old = *src++ >> (w-ptgt);
        len -= w;
    }
    if (len + ptgt > w) *tgt = old;
}

namespace amo{
    // Tamaño recomendado para una hoja nueva
    static uint leafNewSize() {
        return MaxBlockWords * NewFraction;
    }

    // Tamaño máximo para hoja
    static uint leafMaxSize() {
        return MaxBlockWords;
    }

    class LeafBV {
        public:
            uint64_t size;           // número de bits almacenados
            uint64_t ones;           // cantidad de bits en 1
            uint64_t* data;      // arreglo de datos

            // Constructor hoja vacía
            LeafBV() : size(0), ones(0) {
                data = new uint64_t[MaxBlockWords];
            }

            // Constructor con datos externos (copia data)
            LeafBV(const uint64_t* inputData, uint n) : size(n), ones(0) {
                data = new uint64_t[MaxBlockWords];

                // Copiar datos, solo los bytes necesarios
                size_t bytesToCopy = (n + 7) / 8; 
                std::memcpy(data, inputData, bytesToCopy);

                uint nb = (n + w - 1) / w;
                if (n % w) {
                    data[nb - 1] &= (((uint64_t)1) << (n % w)) - 1;
                }
                for (uint i = 0; i < nb; i++) {
                    ones += popcount(data[i]);
                }
            }

            // constructor por copia
            LeafBV(const LeafBV& other) : size(other.size), ones(other.ones) {
                data = new uint64_t[MaxBlockWords];
                std::memcpy(data, other.data, MaxBlockWords * sizeof(uint64_t));
            }

            // Constructor por movimiento
            LeafBV(LeafBV&& other) noexcept
            : size(other.size), ones(other.ones), data(other.data)
            {
                other.data = nullptr;  // transferimos la propiedad del puntero
                other.size = 0;
                other.ones = 0;
            }

            // Destructor
            ~LeafBV() {
                delete[] data;
            }

            // Operador de asignación por movimiento
            LeafBV& operator=(LeafBV&& other) noexcept {
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
            uint64_t serialize(std::ostream &out) const {
                if (this->size != 0) {
                    return myfwrite(this->data, sizeof(uint64_t), (this->size+w-1)/w, out);
                }
            }

            // Cargar desde archivo (binario)
            static LeafBV* load(std::istream& in, uint size) {
                uint64_t *data = new uint64_t[MaxBlockWords];
                myfread (data,sizeof(uint64_t),(size+w-1)/w,in);

                LeafBV *leaf = new LeafBV(data, size);
                delete[] data;
                return leaf;
            }

            // Espacio usado en palabras w-bit
            uint64_t space() const {
                return (sizeof(*this) + sizeof(uint64_t) - 1) / sizeof(uint64_t) + MaxBlockWords;
            }

            // Longitud en bits
            uint64_t length() const {
                return size;
            }

            // Cantidad de unos
            uint64_t getOnes() const {
                return ones;
            }

            // Leer bit i
            uint access_(uint i) const {
                return (data[i/w] >> (i%w)) & 1;
            }

            // Escribir bit i, retorna diferencia en unos (+1, 0, -1)
            int write_(uint i, uint v) {
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
            void insert_(uint i, uint v) {
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
            int remove_(uint i) {
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

            // Leer bits [i..i+l-1] en D a partir de D[j] (función no implementada)
            void read(uint i, uint l, uint64_t* D, uint64_t j) const {
                // Aquí deberías implementar copyBits o similar
                copyBits(D,j,data,i,l);
            }

            // rank1 hasta posición i (inclusive)
            uint rank_(uint i) const {
                int p,ib;
                uint ones = 0;
                ib = ++i/w;
                for (p=0;p<ib;p++) ones += popcount(data[p]);
                if (i%w) ones += popcount(data[p] & ((((uint64_t)1)<<(i%w))-1));
                return ones;
            }

            // select1: posición del j-ésimo 1 (1-based)
            uint select1_(uint j) const {
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
            uint select0_(uint j) const {
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

            // splits a full leaf into two
            // returns a dynamicBV
            DynamicBV* splitLeaf () { //eliminar despues LeafBV
                HybridBV *HB1,*HB2;
                DynamicBV *DB;
                LeafBV LB;
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
    };

    class StaticBV {
        public:
            uint64_t size;      // number of bits
            uint64_t ones;      // number of 1s
            uint64_t* data;     // the bits
            uint64_t* S;        // superblocks
            uint16_t* B;        // blocks
        
            // constructor por defecto
            StaticBV() {}

            // Constructor a partir de datos existentes (data se transfiere, no copia)
            StaticBV(uint64_t* input_data, uint64_t n) : size(n) {
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
            StaticBV(const StaticBV& other) : size(other.size), ones(other.ones) {
                // Copiar data (si existe)
                if (size == 0) {
                    data = nullptr;
                } else {
                    uint64_t nb = (size + 63) / 64;
                    data = new uint64_t[nb];
                    std::memcpy(data, other.data, nb * sizeof(uint64_t));
                }
                // Copiar S (si existe)
                if (other.S != nullptr) {
                    uint64_t s_len = (size + (1ULL << w16) - 1) / (1ULL << w16);
                    S = new uint64_t[s_len];
                    std::memcpy(S, other.S, s_len * sizeof(uint64_t));
                } else {
                    S = nullptr;
                }
                // Copiar B (si existe)
                if (other.B != nullptr) {
                    uint64_t b_len = (size + K * w - 1) / (K * w);
                    B = new uint16_t[b_len];
                    std::memcpy(B, other.B, b_len * sizeof(uint16_t));
                } else {
                    B = nullptr;
                }
            }

            // constructor por movimiento
            StaticBV(StaticBV&& other) noexcept {
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
            ~StaticBV() {
                delete[] data;
                delete[] S;
                delete[] B;
            }

            // operador de copia
            StaticBV& operator=(StaticBV&& other) noexcept {
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
            void staticPreprocess () { 
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
            uint64_t serialize (std::ostream &out) { 
                if (size != 0) {
                    return myfwrite (data,sizeof(uint64_t),(size+w-1)/w, out);
                }
            }

            // loads staticBV's data from file, which must be opened for reading
            // size is the number of bits
            static StaticBV* load (std::istream& in, uint64_t size) {
                uint64_t *data = new uint64_t[(size + w - 1) / w];
                myfread (data,sizeof(uint64_t),(size + w - 1)/w,in);
                
                StaticBV *stat = new StaticBV(data, size);
                return stat;
            }

            // Devuelve puntero a los bits
            uint64_t* bits() const {
                return data;
            }

            // Devuelve el tamaño del bitvector en palabras de w bits
            uint64_t space() const {
                if (!this) return 0;
                uint64_t space = sizeof(StaticBV) * 8 / w;
                if (data) space += (size + w - 1) / w;
                if (B) space += ((size + K * w - 1) / (K * w)) / (w / w16);
                if (S) space += (size + (1ULL << w16) - 1) / (1ULL << w16);
                return space;
            }

            // Devuelve el largo (en bits)
            uint64_t length() const {
                return size;
            }

            // Devuelve la cantidad de unos
            uint64_t getOnes() const {
                return ones;
            }

            // Devuelve el valor del bit en la posición i
            uint access_(uint64_t i) const {
                return (data[i / w] >> (i % w)) & 1;
            }

            // read bits [i..i+l-1] onto D[j..], assumes it is right  
            void read (uint64_t i, uint64_t l, uint64_t * D, uint64_t j) { 
                copyBits(D,j,data,i,l);
            }

            // computes rank(i), zero-based, assumes i is right
            uint64_t rank_(uint64_t i) {
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
            uint64_t select1_(uint64_t j) {
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
                    d = std::max(0LL,i-d);
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
                s = std::min(b+(1<<w16)/(w*K),(n+w*K-1)/(w*K));
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
            uint64_t select0_(uint64_t j) {
                int64_t i,d,b;
                uint p;
                uint64_t word,s,m,n;
                n = size;
                s = (n+(1<<w16)-1)/(1<<w16);
                
                // Define función que calcula ceros acumulados hasta el i-esimo superbloque (S)
                auto SZeros = [&](int64_t idx) -> uint64_t {
                    // zeros = posiciones - unos acumulados
                    return ((idx + 1) * (1ULL << w16)) - S[idx];
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
                    d = std::max(0LL,i-d);
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
                    return (idx + 1) * (w * K) - B[idx];
                };

                j -= SZeros(i);
                p = i < s-1 ? SZeros(i+1)-SZeros(i) : zeros-SZeros(i);
                b = (i << w16) / (w*K);
                s = std::min(b+(1<<w16)/(w*K),(n+w*K-1)/(w*K));
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

            DynamicBV* split (uint64_t i) { //eliminar despues StaticBV
                return splitFrom(data,length(),getOnes(),i);
            }
    };

    class DynamicBV {
        public:
            uint64_t size;
            uint64_t ones;
            uint64_t leaves;
            uint64_t accesses; // since last update
            HybridBV* left;
            HybridBV* right;

            // constructor por defecto
            DynamicBV() {}

            // constructor por copia
            DynamicBV(const DynamicBV& other) {
                size = other.size;
                ones = other.ones;
                leaves = other.leaves;
                accesses = other.accesses;

                // Copia profunda de los hijos (usando el constructor por copia de HybridBV)
                left = new HybridBV(*other.left);
                right = new HybridBV(*other.right);
            }

            // Constructor por movimiento
            DynamicBV(DynamicBV&& other) noexcept
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
            DynamicBV& operator=(DynamicBV&& other) noexcept {
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
            LeafBV* mergeLeaves () { // eliminar dynamic
                LeafBV *LB1,*LB2;

                auto leftLeaf = std::get_if<LeafBV>(&left->bv);
                auto rightLeaf = std::get_if<LeafBV>(&left->bv);
                if (!leftLeaf || !rightLeaf) {
                    throw std::runtime_error("El variant no contiene un LeafBV en left o right");
                }
                LB1 = leftLeaf;
                LB2 = rightLeaf;
                copyBits(LB1->data,LB1->size,LB2->data,0,LB2->size);
                LB1->size += LB2->size;
                LB1->ones += LB2->ones;
                delete LB2;

                return LB1;
            }

            // transfers bits from the right to the left leaves children of B
            // tells if it transfered something
            int transferLeft () { 
                LeafBV *LB1,*LB2;
                uint i,trf,ones,words;
                uint64_t *segment;

                auto leftLeaf = std::get_if<LeafBV>(&left->bv);
                auto rightLeaf = std::get_if<LeafBV>(&left->bv);
                if (!leftLeaf || !rightLeaf) {
                    throw std::runtime_error("El variant no contiene un LeafBV en left o right");
                }

                LB1 = leftLeaf;
                LB2 = rightLeaf;
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
            int transferRight () { 
                LeafBV *LB1,*LB2;
                uint i,trf,ones,words;
                uint64_t *segment;

                auto leftLeaf = std::get_if<LeafBV>(&left->bv);
                auto rightLeaf = std::get_if<LeafBV>(&left->bv);
                if (!leftLeaf || !rightLeaf) {
                    throw std::runtime_error("El variant no contiene un LeafBV en left o right");
                }

                LB1 = leftLeaf;
                LB2 = rightLeaf;
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

            uint64_t getLeaves() const {
                return leaves;
            }

            uint64_t space() const {
                return (sizeof(DynamicBV)*8+w-1)/w + left->space() + right->space();
            }

            // Devuelve el largo (en bits)
            uint64_t length() const {
                return size;
            }

            // Devuelve la cantidad de unos
            uint64_t getOnes() const {
                return ones;
            }
    };

    class HybridBV {
        public:
            std::variant<StaticBV*, LeafBV*, DynamicBV*> bv;
            
            // Constructor por defecto: crea un leaf vacío
            HybridBV() {
                bv = new LeafBV();
            }

            HybridBV(uint64_t* data, uint64_t n) {
                if (n > leafNewSize() * w) {
                    bv = new StaticBV(data, n);
                } else {
                    bv = new LeafBV(data, n);
                }
            }

            // constructor por copia
            HybridBV(const HybridBV& other) {
                bv = std::visit([](auto ptr) -> std::variant<StaticBV*, LeafBV*, DynamicBV*> {
                    using T = std::decay_t<decltype(*ptr)>;
                    return new T(*ptr);  // copia del objeto apuntado
                }, other.bv);
            }

            // constructor por movimiento
            HybridBV(HybridBV&& other) noexcept {
                bv = std::move(other.bv);
                other.bv = static_cast<DynamicBV*>(nullptr); // dejar en estado válido
            }

            ~HybridBV() {
                deleteBV();
            }

            void deleteBV() {
                if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    delete dyn->left;
                    delete dyn->right;
                }

                std::visit([](auto ptr) {
                    delete ptr; 
                }, bv);
            }

            // operador de copia
            HybridBV& operator=(const HybridBV& other) {
                if (this != &other) {
                    deleteBV();  // liberamos el contenido actual
                    bv = std::visit([](auto ptr) -> std::variant<StaticBV*, LeafBV*, DynamicBV*> {
                        using T = std::decay_t<decltype(*ptr)>;
                        return new T(*ptr);
                    }, other.bv);
                }
                return *this;
            }

            // operador de movimiento
            HybridBV& operator=(HybridBV&& other) noexcept {
                if (this != &other) {
                    deleteBV();
                    bv = std::move(other.bv);
                    other.bv = static_cast<DynamicBV*>(nullptr); // dejar en estado válido
                }
                return *this;
            }

            // collects all the descending bits into an array, destroys bv.dyn
            uint64_t* collect (uint64_t len) {
                auto* dyn = std::get_if<DynamicBV>(&bv);
                if (!dyn) {
                    throw std::runtime_error("collect() llamado sobre nodo que no es DynamicBV");
                }

                uint64_t* D = new uint64_t[(len + w - 1) / w];
                read (0,len,D,0);

                delete dyn->left;
                delete dyn->right;
                delete dyn;

                return D;
            }

            // converts into a leaf if it's short or into a static otherwise
            // delta gives the difference in leaves (new - old)
            void flatten (int64_t *delta) { 
                uint64_t len;
                uint64_t *D;

                auto* dyn = std::get_if<DynamicBV>(&bv);
                if (!dyn) return;
                len = length();
                *delta = - leaves();
                D = collect(len);
                // creates a static
                if (len > leafNewSize()*w) { 
                    bv = new StaticBV(D,len);
                } else { 
                    bv = new LeafBV(D,len);
                }
                *delta += leaves();
            }

            // creates a static version of B, rewriting it but not its address
            // version of hybridRead that does not count accesses, for internal use
            void read (uint64_t i, uint64_t l, uint64_t *D, uint64_t j) { 
                uint64_t lsize;
                if (auto leaf = std::get_if<LeafBV>(&bv)) { 
                    leaf->read(i,l,D,j); 
                    return; 
                }
                if (auto dyn = std::get_if<DynamicBV>(&bv)) { 
                    lsize = dyn->left->length();
                    if (i+l < lsize) {
                        dyn->left->read(i,l,D,j);
                    } else if (i >= lsize) {
                        dyn->right->read(i-lsize,l,D,j);
                    } else { 
                        dyn->left->read(i,lsize-i,D,j);
                        dyn->right->read(0,l-(lsize-i),D,j+(lsize-i));
                    }
                    return;
                }
                if (auto stat = std::get_if<StaticBV>(&bv)) {
                    stat->read(i,l,D,j);
                }
            }

            void balance (uint64_t i, int64_t *delta) { 
                uint64_t len = length();
                uint64_t ones = getOnes();
                uint64_t *D;
                *delta = - leaves();
                D = collect(len);
                bv = splitFrom(D,len,ones,i);
                *delta += leaves();
                delete[] D;
            }

            // writes B to file, which must be opened for writing
            uint64_t serialize(std::ostream &out) {
                int64_t w_bytes = 0;
                int64_t delta;
                flatten(&delta);
                if (auto stat = std::get_if<StaticBV>(&bv)) {
                    w_bytes += myfwrite(&stat->size,sizeof(uint64_t),1,out);
                    w_bytes += stat->serialize(out);
                } else if (auto leaf = std::get_if<LeafBV>(&bv)) { 
                    w_bytes += myfwrite (&leaf->size,sizeof(uint64_t),1,out);
                    w_bytes += leaf->serialize(out);
                } else {
                    throw std::runtime_error("HybridBV::save(): tipo inesperado, se esperaba StaticBV o LeafBV");
                }
                return w_bytes;
            }

            // loads hybridBV from file, which must be opened for reading
            void load(std::istream& in) {
                uint64_t size;
                // Se elimina el anterior bitVector
                deleteBV();
                // Se carga el nuevo bitVector
                myfread (&size,sizeof(uint64_t),1,in);
                if (size > leafNewSize()*w) { 
                    bv = StaticBV::load(in,size);
                } else { 
                    bv = LeafBV::load(in,size);
                }
            }

            // gives space of hybridBV in w-bit words
            uint64_t space () { 
                return std::visit([](auto& obj) -> uint64_t {
                    uint64_t s = (sizeof(HybridBV) * 8 + w - 1) / w;
                    return s + obj->space();
                }, bv);
            }

            // gives number of leaves
            uint64_t leaves () { 
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    return 1;
                } else if (auto stat = std::get_if<StaticBV>(&bv)) {
                    return (stat->length()+leafNewSize()*w-1) / (leafNewSize()*w);
                } else if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    return dyn->getLeaves();
                }
            }

            // gives bit length
            uint64_t length() { 
                return std::visit([](auto& obj) -> uint64_t {
                    return obj->length();
                }, bv);
            }

            // gives number of ones
            uint64_t getOnes() {
                return std::visit([](auto& obj) -> uint64_t {
                    return obj->getOnes();
                }, bv);
            }

            // sets value for B[i]= (v != 0), assumes i is right
            // returns the difference in 1s
            int write_(uint64_t i, uint v) { 
                uint64_t lsize;
                int dif;
                if (auto stat = std::get_if<StaticBV>(&bv)) { 
                    // does not change #leaves!
                    bv = stat->split(i);
                    delete stat;
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    return leaf->write_(i,v);
                } else if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    dyn->accesses = 0; // reset
                    lsize = dyn->left->length();
                    if (i < lsize) {
                        dif = dyn->left->write_(i,v);
                    } else {
                        dif = dyn->right->write_(i-lsize,v);
                    }
                    dyn->ones += dif;
                    return dif;
                }
            }

            // changing leaves is uncommon and only then we need to recompute
            // leaves. we do our best to avoid this overhead in typical operations
            void irecompute (uint64_t i) { 
                uint64_t lsize;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    lsize = dyn->left->length();
                    if (i < lsize) {
                        dyn->left->irecompute(i);
                    } else {
                        dyn->right->irecompute(i-lsize);
                    }
                    dyn->leaves = dyn->left->leaves() + dyn->right->leaves();
                }
            }

            void rrecompute (uint64_t i, uint64_t l) { 
                uint64_t lsize;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    lsize = dyn->left->length();
                    if (i+l < lsize) {
                        dyn->left->rrecompute(i,l);
                    } else if (i >= lsize) {
                        dyn->right->rrecompute(i-lsize,l);
                    } else { 
                        dyn->left->rrecompute(i,lsize-i);
                        dyn->right->rrecompute(0,l-(lsize-i));
                    }
                    dyn->leaves = dyn->left->leaves() + dyn->right->leaves();
                }
            }

            // inserts v at B[i], assumes i is right
            void insert_(uint64_t i, uint v, uint *recalc) { 
                uint64_t lsize,rsize;
                int64_t delta;
                if (auto stat = std::get_if<StaticBV>(&bv)) { 
                    // does not change #leaves!
                    bv = stat->split(i);
                    delete stat;
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    // split
                    if (leaf->length() == leafMaxSize() * w) { 
                        bv = leaf->splitLeaf();
                        delete leaf;
                        *recalc = 1; // leaf added
                    } else { 
                        leaf->insert_(i,v);
                        return;
                    }
                }
                if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    dyn->accesses = 0; // reset
                    lsize = dyn->left->length();
                    rsize = dyn->right->length();
                    // insert on left child
                    if (i < lsize) {
                        if ((lsize == leafMaxSize() * w)     // will overflow if leaf
                            && (rsize < leafMaxSize() * w)  // can avoid if leaf
                            && std::get_if<LeafBV*>(&(dyn->left->bv)) // both are leaves
                            && std::get_if<LeafBV*>(&(dyn->right->bv)) 
                            && dyn->transferRight()) // avoided, transferred to right
                        {
                            insert_(i,v,recalc); // now could be to the right!
                            return;
                        }
                        if ((lsize+1 > AlphaFactor*(lsize+rsize+1))
                            && (lsize+rsize >= MinLeavesToBalance*leafMaxSize()*w) 
                            && canBalance(lsize+rsize,1,0)) // too biased
                        {
                            delta = 0;
                            balance(i,&delta);
                            if (delta) {
                                *recalc = 1;
                            }
                            insert_(i,v,recalc); 
                            return;
                        }
                        dyn->left->insert_(i,v,recalc); // normal recursive call 
                    // insert on right child
                    } else {
                        if ((rsize == leafMaxSize() * w)    // will overflow if leaf
                            && (lsize < leafMaxSize() * w) // can avoid if leaf
                            && std::get_if<LeafBV*>(&(dyn->left->bv)) // both are leaves
                            && std::get_if<LeafBV*>(&(dyn->right->bv)) 
                            && dyn->transferLeft()) // avoided, transferred to right
                        {
                            insert_(i,v,recalc); // now could be to the left!
                            return;
                        }
                        if ((rsize+1 > AlphaFactor*(lsize+rsize+1))
                            && (lsize+rsize >= MinLeavesToBalance*leafMaxSize()*w) 
                            && canBalance(lsize+rsize,0,1)) // too biased
                        {
                            delta = 0;
                            balance(i,&delta);
                            if (delta) {
                                *recalc = 1;
                            }
                            insert_(i,v,recalc);
                            return;
                        }
                        dyn->right->insert_(i-lsize,v,recalc); // normal rec call
                    }
                    dyn->size++;
                    dyn->ones += v;
                }
            }

            void hybridInsert_(uint64_t i, uint v) { 
                uint recalc = 0;
                insert_(i,v,&recalc);
                // we went to the leaf now holding i
                if (recalc) {
                    irecompute(i);
                }
            }

            // deletes B[i], assumes i is right
            // returns difference in 1s
            int remove_(uint64_t i, uint *recalc) { 
                uint64_t lsize,rsize;
                HybridBV* HB;
                int dif;
                int64_t delta;
                if (auto stat = std::get_if<StaticBV>(&bv)) { 
                    // does not change #leaves!
                    bv = stat->split(i);
                    delete stat;
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    return leaf->remove_(i);
                }
                if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    // reset
                    dyn->accesses = 0;
                    lsize = dyn->left->length();
                    rsize = dyn->right->length();
                    if (i < lsize) {
                        if ((rsize > AlphaFactor*(lsize+rsize-1))
                        && (lsize+rsize >= MinLeavesToBalance*leafMaxSize()*w) 
                        && canBalance(lsize+rsize,-1,0)) // too biased
                        {
                            delta = 0;
                            balance(i,&delta); 
                            if (delta) *recalc = 1;
                            return remove_(i,recalc); // now could enter in the right child!
                        }
                        dif = dyn->left->remove_(i,recalc); // normal recursive call otherw
                        // left child is now of size zero, remove
                        if (lsize == 1) {
                            bv = dyn->right->bv;
                            dyn->right->bv = new LeafBV(); 
                            delete dyn->right;
                            delete dyn->left;
                            delete dyn;
                            *recalc = 1; 
                            return dif;
                        } 
                    } else {
                        if ((lsize > AlphaFactor*(lsize+rsize-1))
                        && (lsize+rsize >= MinLeavesToBalance*leafMaxSize()*w) 
                        && canBalance(lsize+rsize,0,-1)) // too biased
                        {
                            delta = 0;
                            balance(i,&delta);
                            if (delta) *recalc = 1; 
                            return remove_(i,recalc); // now could enter in the left child!
                        }
                        dif = dyn->right->remove_(i-lsize,recalc); // normal recursive call
                        // right child now size zero, remove
                        if (rsize == 1) {
                            bv = dyn->left->bv;
                            dyn->left->bv = new LeafBV();
                            delete dyn->right;
                            delete dyn->left;
                            delete dyn;
                            *recalc = 1; 
                            return dif;
                        }
                    }
                    dyn->size--;
                    dyn->ones += dif;
                    // merge, must be leaves
                    if (dyn->size <= leafNewSize() * w) {
                        bv = dyn->mergeLeaves();
                        delete dyn;
                        *recalc = 1; 
                    }
                    else if (dyn->size < dyn->leaves * leafNewSize() * w * MinFillFactor) {
                        delta = 0;
                        flatten(&delta); 
                        if (delta) {
                            *recalc = 1;
                        }
                    }
                    return dif;
                }
            }

            int hybridRemove_(uint64_t i) { 
                uint recalc = 0;
                int dif = remove_(i,&recalc);
                // the node is now at i-1 or at i, hard to know
                if (recalc) {
                    irecompute(i-1);
                    irecompute(i); 
                }
                return dif;
            }

            // flattening is uncommon and only then we need to recompute
            // leaves. we do our best to avoid this overhead in typical queries
            void recompute (uint64_t i, int64_t delta) { 
                uint64_t lsize;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    dyn->leaves += delta;
                    lsize = dyn->left->length();
                    if (i < lsize) {
                        dyn->left->recompute(i,delta);
                    } else {
                        dyn->right->recompute(i-lsize,delta);
                    }
                }
            }

            bool mustFlatten() {
                if (auto dyn = std::get_if<DynamicBV>(&bv)) {
                    return (dyn->accesses >= FactorBV*dyn->size);
                }
            }

            // access B[i], assumes i is right
            uint access_ (uint64_t i, int64_t *delta) { 
                uint64_t lsize;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) { 
                    dyn->accesses++;
                    if (mustFlatten()) {
                        flatten(delta);
                    } else { 
                        lsize = dyn->left->length();
                        if (i < lsize) {
                            return dyn->left->access_(i,delta);
                        } else {
                            return dyn->right->access_(i-lsize,delta);
                        }
                    }
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    return leaf->access_(i);
                } else if (auto stat = std::get_if<StaticBV>(&bv)) {
                    return stat->access_(i);
                }
            }

            uint hybridAccess_(uint64_t i) { 
                int64_t delta = 0;
                uint answ = access_(i,&delta);
                if (delta) {
                    recompute(i,delta);
                }
                return answ;
            }

            // read bits [i..i+l-1], onto D[j...]
            void read (uint64_t i, uint64_t l, uint64_t *D, uint64_t j, uint *recomp) { 
                uint64_t lsize;
                int64_t delta;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) { 
                    dyn->accesses++;
                    if (mustFlatten()) {
                        delta = 0;
                        flatten(&delta); 
                        if (delta) *recomp = 1;
                    } else {
                        lsize = dyn->left->length();
                        if (i+l < lsize) {
                            dyn->left->read(i,l,D,j,recomp);
                        } else if (i>=lsize) {
                            dyn->right->read(i-lsize,l,D,j,recomp);
                        } else { 
                            dyn->left->read(i,lsize-i,D,j,recomp);
                            dyn->right->read(0,l-(lsize-i),D,j+(lsize-i),recomp);
                        }
                        return;
                    }
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) { 
                    leaf->read(i,l,D,j);
                    return;
                } else if (auto stat = std::get_if<StaticBV>(&bv)) {
                    stat->read(i,l,D,j);
                }
            }

            void hybridRead (uint64_t i, uint64_t l, uint64_t *D, uint64_t j) { 
                uint recomp = 0;
                read(i,l,D,j,&recomp);
                if (recomp) {
                    rrecompute(i,l);
                }
            }

            // computes rank_1(B,i), zero-based, assumes i is right
            uint64_t rank_(uint64_t i, int64_t *delta) { 
                uint64_t lsize;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) { 
                    dyn->accesses++;
                    if (mustFlatten()) {
                        flatten(delta); 
                    } else { 
                        lsize = dyn->left->length();
                        if (i < lsize) { 
                            return dyn->left->rank_(i,delta);
                        } else {
                        return dyn->left->getOnes() + dyn->right->rank_(i-lsize,delta);
                        }
                    }
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    return leaf->rank_(i);
                } else if (auto stat = std::get_if<StaticBV>(&bv)) {
                    return stat->rank_(i);
                }
            }

            uint64_t hybridRank_(uint64_t i) { 
                int64_t delta = 0;
                uint64_t answ = rank_(i,&delta);
                if (delta) {
                    recompute(i,delta);
                }
                return answ;
            }

            // computes select_1(B,j), zero-based, assumes j is right
            uint64_t select1_(uint64_t j, int64_t *delta) { 
                uint64_t lones;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) { 
                    dyn->accesses++;
                    if (mustFlatten()) {
                        flatten(delta); 
                    } else { 
                        lones = dyn->left->getOnes();
                        if (j <= lones) {
                            return dyn->left->select1_(j,delta);
                        }
                        return dyn->left->length() + dyn->right->select1_(j-lones,delta);
                    }
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    return leaf->select1_(j);   
                } else if (auto stat = std::get_if<StaticBV>(&bv)) {
                    return stat->select1_(j);
                }
            }

            // computes select_0(B,j), zero-based, assumes j is right
            uint64_t select0_(uint64_t j, int64_t *delta) { 
                uint64_t lzeros;
                if (auto dyn = std::get_if<DynamicBV>(&bv)) { 
                    dyn->accesses++;
                    if (mustFlatten()) {
                        flatten(delta); 
                    } else { 
                        lzeros = dyn->left->length() - dyn->left->getOnes();
                        if (j <= lzeros) {
                            return dyn->left->select0_(j,delta);
                        }
                        return dyn->left->length() + dyn->right->select0_(j-lzeros,delta);
                    }
                }
                if (auto leaf = std::get_if<LeafBV>(&bv)) {
                    return leaf->select0_(j);   
                } else if (auto stat = std::get_if<StaticBV>(&bv)) {
                    return stat->select0_(j);
                }
            }

            uint64_t hybridSelect1_(uint64_t j) { 
                int64_t delta = 0;
                uint64_t answ = select1_(j,&delta);
                if (delta) {
                    recompute(answ,delta);
                }
                return answ;
            }

            uint64_t hybridSelect0_(uint64_t j) { 
                int64_t delta = 0;
                uint64_t answ = select0_(j,&delta);
                if (delta) {
                    recompute(answ,delta);
                }
                return answ;
            }

            uint64_t at(uint64_t i) {
                return hybridAccess_(i);
            }

            uint64_t rank(uint64_t i, bool b) {
                uint64_t r1 = hybridRank_(i);

                return b ? r1 : i - r1;
            }

            uint64_t select1(uint64_t i) {
                return hybridSelect1_(i);
            }

            uint64_t select0(uint64_t i) {
                return hybridSelect0_(i);
            }

            uint64_t select(uint64_t i, bool b) {
                return b ? select1(i) : select0(i);
            }

            void insert(uint64_t i, uint v) {
                hybridInsert_(i, v);
            }

            void insert0(uint64_t i) {
                insert(i, 0);
            }

            void insert1(uint64_t i){
                insert(i, 1);
            }

            int remove(uint64_t i) {
                return hybridRemove_(i);
            }

            int set(uint64_t i, bool b = true) {
                return write_(i, b);
            }
    };

    // halves a static bitmap into leaves, leaving a leaf covering i
    // returns a dynamicBV and destroys B
    static DynamicBV* splitFrom (uint64_t *data, uint64_t n, uint64_t ones, uint64_t i) { 
        HybridBV *HB;
        DynamicBV *DB,*finalDB;
        uint bsize; // byte size of blocks to create
        uint blen; // bit size of block to create
        uint64_t *segment;
        uint64_t nblock;
        unsigned char *start,*mid,*end;

        HB = NULL;
        blen = leafNewSize() * w;
        bsize = (blen+7)/8; // +7 not really needed as 8 | w
        nblock = (n+blen-1)/blen; // total blocks 
        start = (unsigned char*)data;
        end = start + (n+7)/8;
        while (nblock >= 2) {
            DB = new DynamicBV();
            if (HB == NULL) {
                finalDB = DB;
            } else { 
                HB->bv = DB;
            }
            DB->size = n;
            DB->ones = ones;
            DB->leaves = nblock;
            DB->accesses = 0;
            mid = start+(nblock/2)*bsize;
            // split the left half
            if (i < (nblock/2)*blen) {
                // create right half
                DB->right = HB = new HybridBV();
                // create a static
                if (n - (nblock/2)*blen > leafNewSize() * w) {
                    segment = new uint64_t[(end - mid) / sizeof(uint64_t)];
                    memcpy(segment,mid,end-mid);
                    HB->bv = new StaticBV(segment,n-(nblock/2)*blen);
                // create a leaf
                } else {
                    HB->bv = new LeafBV((uint64_t*)mid,n-(nblock/2)*blen);
                }
                // continue on left half
                end = mid;
                nblock = nblock/2;
                n = nblock * blen;
                ones -= HB->getOnes();
                DB->left = HB = new HybridBV();
            // split the right half
            } else {
                // create left half
                DB->left = HB = new HybridBV();
                // create a static
                if ((nblock/2)*blen > leafNewSize() * w) {
                    segment = new uint64_t[(end - start) / sizeof(uint64_t)];
                    memcpy(segment,start,mid-start);
                    HB->bv = new StaticBV(segment,(nblock/2)*blen);
                // create a leaf
                } else {
                    HB->bv = new LeafBV((uint64_t*)start,(nblock/2)*blen);
                }
                // continue for right half
                start = mid;
                n = n-(nblock/2)*blen;
                i = i-(nblock/2)*blen;
                ones -= HB->getOnes();
                nblock = nblock - nblock/2;
                DB->right = HB = new HybridBV();
            }
        }
        // finally, the leaf where i lies
        HB->bv = new LeafBV((uint64_t*)start,n);
        return finalDB;
    }

    // balance by rebuilding: flattening + splitting
    // assumes B is dynamic
    static inline int canBalance (uint64_t n, int dleft, int dright) { 
        uint b = leafNewSize() * w; // bit size of leaves to create in split
        uint64_t left = (((n+b-1)/b)/2)*b; // bit size of left part 
        uint64_t right = n-left; // bit size of right part 
    //     if (left+dleft < (1-AlphaFactor)*(n+dleft+dright)) return 0;
    //     if (right+dright < (1-AlphaFactor)*(n+dleft+dright)) return 0;
        if (left+dleft > AlphaFactor*(n+dleft+dright)) return 0;
        if (right+dright > AlphaFactor*(n+dleft+dright)) return 0;
        return 1;
    }
}
