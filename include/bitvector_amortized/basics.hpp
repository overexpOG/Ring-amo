#ifndef BITVECTOR_AMORTIZED_BASICS
#define BITVECTOR_AMORTIZED_BASICS

#include <iostream>
#include <cstdint>
#include <cstring>

namespace amo {
    static float Theta = 0.01;                             // Theta * length reads => rebuild as static
    static float Epsilon = 0.1;                            // do not flatten leaves of size over Epsilon * n
    static constexpr int MaxBlockWords = 32;
    static constexpr double NewFraction = 0.75;
    static constexpr int w = 64;                           // bits por palabra
    static constexpr int K = 4;                            // block length is w*K
    static constexpr size_t w16 = 8 * sizeof(uint16_t);    // superblock length is 2^w16
    static constexpr float TrfFactor = 0.125;              // TrfFactor * MaxLeafSize to justify transferLeft/Right
    static constexpr float AlphaFactor = 0.65;             // balance factor 3/5 < . < 1
    static constexpr int MinLeavesToBalance = 5;            // min number of leaves to balance the tree
    static constexpr float MinFillFactor = 0.3;             // less than this involves rebuild. Must be <= NewFraction/2

    using uint = uint32_t;

    inline void setTheta(float t) {
        Theta = t;
    }

    inline uint popcount (uint64_t y) { 
        y -= ((y >> 1) & 0x5555555555555555ull);
        y = (y & 0x3333333333333333ull) + (y >> 2 & 0x3333333333333333ull);
        return ((y + (y >> 4)) & 0xf0f0f0f0f0f0f0full) * 0x101010101010101ull >> 56;
    }

    inline void myfread(void* ptr, size_t size, size_t nmemb, std::istream& in) {
        // Leer del stream
        in.read(static_cast<char*>(ptr), size * nmemb);

        // Verificar la cantidad de datos realmente leídos
        if (in.gcount() != static_cast<std::streamsize>(size * nmemb)) {
            std::cerr << "Error: fread of " << nmemb * size << " bytes failed\n";
            std::exit(1);
        }
    }

    inline uint64_t myfwrite(void* ptr, size_t size, size_t nmemb, std::ostream& out) {
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
    inline void copyBits (uint64_t* tgt, uint64_t ptgt, uint64_t* src, uint64_t psrc, uint64_t len) { 
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
            memcpy(tgt,src,((len+w-1)/w)*sizeof(uint64_t));
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

    // Tamaño recomendado para una hoja nueva
    static inline uint leafNewSize() {
        return MaxBlockWords * NewFraction;
    }

    // Tamaño máximo para hoja
    static inline uint leafMaxSize() {
        return MaxBlockWords;
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

#endif
