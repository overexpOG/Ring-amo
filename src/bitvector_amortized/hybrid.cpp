#include <variant>
#include "bitvector_amortized/dynamic.hpp"
#include "bitvector_amortized/hybrid.hpp"
#include "bitvector_amortized/leaf.hpp"
#include "bitvector_amortized/static.hpp"

namespace amo {
    // Constructor por defecto: crea un leaf vacío
    HybridBV::HybridBV() {
        bv = new LeafBV();
    }

    HybridBV::HybridBV(uint64_t* data, uint64_t n) {
        if (n > leafNewSize() * w) {
            bv = new StaticBV(data, n);
        } else {
            bv = new LeafBV(data, n);
        }
    }

    // constructor por copia
    HybridBV::HybridBV(const HybridBV& other) {
        bv = std::visit([](auto ptr) -> std::variant<StaticBV*, LeafBV*, DynamicBV*> {
            using T = std::decay_t<decltype(*ptr)>;
            return new T(*ptr);  // copia del objeto apuntado
        }, other.bv);
    }

    // constructor por movimiento
    HybridBV::HybridBV(HybridBV&& other) noexcept {
        bv = std::move(other.bv);
        other.bv = static_cast<LeafBV*>(nullptr); // dejar en estado válido
    }

    HybridBV::~HybridBV() {
        deleteBV();
    }

    void HybridBV::deleteBV() {
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            delete (*dyn)->left;
            delete (*dyn)->right;
        }

        std::visit([](auto ptr) {
            delete ptr; 
        }, bv);
    }

    // operador de copia
    HybridBV& HybridBV::operator=(const HybridBV& other) {
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
    HybridBV& HybridBV::operator=(HybridBV&& other) noexcept {
        if (this != &other) {
            deleteBV();
            bv = std::move(other.bv);
            other.bv = static_cast<LeafBV*>(nullptr); // dejar en estado válido
        }
        return *this;
    }

    // collects all the descending bits into an array, destroys bv.dyn
    uint64_t* HybridBV::collect(uint64_t len) {
        auto* dyn = std::get_if<DynamicBV*>(&bv);
        if (!dyn) {
            throw std::runtime_error("collect() llamado sobre nodo que no es DynamicBV");
        }

        uint64_t* D = new uint64_t[(len + w - 1) / w]();
        read(0,len,D,0);

        delete (*dyn)->left;
        delete (*dyn)->right;
        delete (*dyn);

        return D;
    }

    // converts into a leaf if it's short or into a static otherwise
    // delta gives the difference in leaves (new - old)
    void HybridBV::flatten(int64_t *delta) {
        uint64_t len;
        uint64_t *D;

        auto dyn = std::get_if<DynamicBV*>(&bv);
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
        delete[] D;
        *delta += leaves();
    }

    // creates a static version of B, rewriting it but not its address
    // version of hybridRead that does not count accesses, for internal use
    void HybridBV::read(uint64_t i, uint64_t l, uint64_t *D, uint64_t j) {
        uint64_t lsize;
        if (auto leaf = std::get_if<LeafBV*>(&bv)) { 
            (*leaf)->read(i,l,D,j); 
            return; 
        }
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) { 
            lsize = (*dyn)->left->length();
            if (i+l < lsize) {
                (*dyn)->left->read(i,l,D,j);
            } else if (i >= lsize) {
                (*dyn)->right->read(i-lsize,l,D,j);
            } else { 
                (*dyn)->left->read(i,lsize-i,D,j);
                (*dyn)->right->read(0,l-(lsize-i),D,j+(lsize-i));
            }
            return;
        }
        if (auto stat = std::get_if<StaticBV*>(&bv)) {
            (*stat)->read(i,l,D,j);
        }
    }

    // halves a static bitmap into leaves, leaving a leaf covering i
    // returns a dynamicBV and destroys B
    DynamicBV* HybridBV::splitFrom (uint64_t *data, uint64_t n, uint64_t ones, uint64_t i) {
        HybridBV *HB;
        DynamicBV *DB,*finalDB;
        uint bsize; // byte size of blocks to create
        uint blen; // bit size of block to create
        uint subBits;
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
                subBits = n - (nblock/2)*blen;
                // create right half
                DB->right = HB = new HybridBV();
                // create a static
                if (n - (nblock/2)*blen > leafNewSize() * w) {
                    HB->bv = new StaticBV((uint64_t*)mid, subBits);
                // create a leaf
                } else {
                    HB->bv = new LeafBV((uint64_t*)mid, subBits);
                }
                // continue on left half
                end = mid;
                nblock = nblock/2;
                n = nblock * blen;
                ones -= HB->getOnes();
                DB->left = HB = new HybridBV();
            // split the right half
            } else {
                subBits =(nblock/2)*blen;
                // create left half
                DB->left = HB = new HybridBV();
                // create a static
                if ((nblock/2)*blen > leafNewSize() * w) {
                    HB->bv = new StaticBV((uint64_t*)start,subBits);
                // create a leaf
                } else {
                    HB->bv = new LeafBV((uint64_t*)start,subBits);
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

    void HybridBV::balance(uint64_t i, int64_t *delta) { 
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
    uint64_t HybridBV::serialize(std::ostream &out) {
        int64_t w_bytes = 0;
        int64_t delta;
        flatten(&delta);
        if (auto stat = std::get_if<StaticBV*>(&bv)) {
            w_bytes += myfwrite(&(*stat)->size,sizeof(uint64_t),1,out);
            w_bytes += (*stat)->serialize(out);
        } else if (auto leaf = std::get_if<LeafBV*>(&bv)) { 
            w_bytes += myfwrite (&(*leaf)->size,sizeof(uint64_t),1,out);
            w_bytes += (*leaf)->serialize(out);
        } else {
            throw std::runtime_error("HybridBV::save(): tipo inesperado, se esperaba StaticBV o LeafBV");
        }
        return w_bytes;
    }

    // loads hybridBV from file, which must be opened for reading
    void HybridBV::load(std::istream& in) {
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
    uint64_t HybridBV::space () const { 
        return std::visit([](auto& obj) -> uint64_t {
            uint64_t s = (sizeof(HybridBV) * 8 + w - 1) / w;
            return s + obj->space();
        }, bv);
    }

    // gives number of leaves
    uint64_t HybridBV::leaves () const {
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return 1;
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            return ((*stat)->length()+leafNewSize()*w-1) / (leafNewSize()*w);
        } else if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            return (*dyn)->getLeaves();
        }
        throw std::runtime_error("HybridBV::leaves(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    // gives bit length
    uint64_t HybridBV::length() const { 
        return std::visit([](auto& obj) -> uint64_t {
            return obj->length();
        }, bv);
    }

    // gives number of ones
    uint64_t HybridBV::getOnes() const {
        return std::visit([](auto& obj) -> uint64_t {
            return obj->getOnes();
        }, bv);
    }

    // gives the type of bv
    const char* HybridBV::getType() const { 
        return std::visit([](auto& obj) -> const char* {
            return obj->getType();
        }, bv);
    }

    // sets value for B[i]= (v != 0), assumes i is right
    // returns the difference in 1s
    int HybridBV::write_(uint64_t i, uint v) { 
        uint64_t lsize;
        int dif;
        if (auto stat = std::get_if<StaticBV*>(&bv)) { 
            // does not change #leaves!
            StaticBV* ptr = *stat;
            bv = ptr->split(i);
            delete ptr;
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->write_(i,v);
        } else if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            (*dyn)->accesses = 0; // reset
            lsize = (*dyn)->left->length();
            if (i < lsize) {
                dif = (*dyn)->left->write_(i,v);
            } else {
                dif = (*dyn)->right->write_(i-lsize,v);
            }
            (*dyn)->ones += dif;
            return dif;
        }
        throw std::runtime_error("HybridBV::write_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    // changing leaves is uncommon and only then we need to recompute
    // leaves. we do our best to avoid this overhead in typical operations
    void HybridBV::irecompute (uint64_t i) { 
        uint64_t lsize;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            lsize = (*dyn)->left->length();
            if (i < lsize) {
                (*dyn)->left->irecompute(i);
            } else {
                (*dyn)->right->irecompute(i-lsize);
            }
            (*dyn)->leaves = (*dyn)->left->leaves() + (*dyn)->right->leaves();
        }
    }

    void HybridBV::rrecompute (uint64_t i, uint64_t l) { 
        uint64_t lsize;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            lsize = (*dyn)->left->length();
            if (i+l < lsize) {
                (*dyn)->left->rrecompute(i,l);
            } else if (i >= lsize) {
                (*dyn)->right->rrecompute(i-lsize,l);
            } else { 
                (*dyn)->left->rrecompute(i,lsize-i);
                (*dyn)->right->rrecompute(0,l-(lsize-i));
            }
            (*dyn)->leaves = (*dyn)->left->leaves() + (*dyn)->right->leaves();
        }
    }

    // inserts v at B[i], assumes i is right
    void HybridBV::insert_(uint64_t i, uint v, uint *recalc) { 
        uint64_t lsize,rsize;
        int64_t delta;
        if (auto stat = std::get_if<StaticBV*>(&bv)) { 
            // does not change #leaves!
            StaticBV* old_stat = *stat;
            bv = old_stat->split(i);
            delete old_stat;
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            // split
            if ((*leaf)->length() == leafMaxSize() * w) {
                LeafBV* old_leaf = *leaf;
                bv = old_leaf->splitLeaf();
                delete old_leaf;
                *recalc = 1; // leaf added
            } else { 
                (*leaf)->insert_(i,v);
                return;
            }
        }
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            (*dyn)->accesses = 0; // reset
            lsize = (*dyn)->left->length();
            rsize = (*dyn)->right->length();
            // insert on left child
            if (i < lsize) {
                if ((lsize == leafMaxSize() * w)     // will overflow if leaf
                    && (rsize < leafMaxSize() * w)  // can avoid if leaf
                    && std::get_if<LeafBV*>(&((*dyn)->left->bv)) // both are leaves
                    && std::get_if<LeafBV*>(&((*dyn)->right->bv)) 
                    && (*dyn)->transferRight()) // avoided, transferred to right
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
                (*dyn)->left->insert_(i,v,recalc); // normal recursive call 
            // insert on right child
            } else {
                if ((rsize == leafMaxSize() * w)    // will overflow if leaf
                    && (lsize < leafMaxSize() * w) // can avoid if leaf
                    && std::get_if<LeafBV*>(&((*dyn)->left->bv)) // both are leaves
                    && std::get_if<LeafBV*>(&((*dyn)->right->bv)) 
                    && (*dyn)->transferLeft()) // avoided, transferred to right
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
                (*dyn)->right->insert_(i-lsize,v,recalc); // normal rec call
            }
            (*dyn)->size++;
            (*dyn)->ones += v;
        }
    }

    void HybridBV::hybridInsert_(uint64_t i, uint v) { 
        uint recalc = 0;
        insert_(i,v,&recalc);
        // we went to the leaf now holding i
        if (recalc) {
            irecompute(i);
        }
    }

    // deletes B[i], assumes i is right
    // returns difference in 1s
    int HybridBV::remove_(uint64_t i, uint *recalc) {
        uint64_t lsize,rsize;
        HybridBV* HB;
        int dif;
        int64_t delta;
        if (auto stat = std::get_if<StaticBV*>(&bv)) {
            // does not change #leaves!
            StaticBV* ptr = *stat;
            bv = ptr->split(i);
            delete ptr;
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->remove_(i);
        }
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            // reset
            (*dyn)->accesses = 0;
            lsize = (*dyn)->left->length();
            rsize = (*dyn)->right->length();
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
                dif = (*dyn)->left->remove_(i,recalc); // normal recursive call otherw
                // left child is now of size zero, remove
                if (lsize == 1) {
                    DynamicBV* old_dynamic = *dyn;
                    bv = old_dynamic->right->bv;
                    old_dynamic->right->bv = static_cast<LeafBV*>(nullptr);
                    delete old_dynamic->right;
                    delete old_dynamic->left;
                    delete old_dynamic;
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
                dif = (*dyn)->right->remove_(i-lsize,recalc); // normal recursive call
                // right child now size zero, remove
                if (rsize == 1) {
                    DynamicBV* old_dynamic = *dyn;
                    bv = old_dynamic->left->bv;
                    old_dynamic->left->bv = static_cast<LeafBV*>(nullptr);
                    delete old_dynamic->right;
                    delete old_dynamic->left;
                    delete old_dynamic;
                    *recalc = 1;
                    return dif;
                }
            }
            (*dyn)->size--;
            (*dyn)->ones += dif;
            // merge, must be leaves
            if ((*dyn)->size <= leafNewSize() * w) {
                DynamicBV* old_dynamic = *dyn;
                bv = old_dynamic->mergeLeaves();
                delete old_dynamic;
                *recalc = 1; 
            }
            else if ((*dyn)->size < (*dyn)->leaves * leafNewSize() * w * MinFillFactor) {
                delta = 0;
                flatten(&delta); 
                if (delta) {
                    *recalc = 1;
                }
            }
            return dif;
        }
        throw std::runtime_error("HybridBV::remove_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    int HybridBV::hybridRemove_(uint64_t i) { 
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
    void HybridBV::recompute (uint64_t i, int64_t delta) { 
        uint64_t lsize;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            (*dyn)->leaves += delta;
            lsize = (*dyn)->left->length();
            if (i < lsize) {
                (*dyn)->left->recompute(i,delta);
            } else {
                (*dyn)->right->recompute(i-lsize,delta);
            }
        }
    }

    bool HybridBV::mustFlatten(uint64_t n) {
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            return (((*dyn)->size <= Epsilon * n) && ((*dyn)->accesses >= Theta*(*dyn)->size));
        }
        throw std::runtime_error("HybridBV::mustFlatten(): tipo inesperado, se esperaba DynamicBV");
    }

    // access B[i], assumes i is right
    uint HybridBV::access_ (uint64_t i, int64_t *delta, uint64_t n) { 
        uint64_t lsize;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) { 
            (*dyn)->accesses++;
            if (mustFlatten(n)) {
                flatten(delta);
            } else { 
                lsize = (*dyn)->left->length();
                if (i < lsize) {
                    return (*dyn)->left->access_(i,delta, n);
                } else {
                    return (*dyn)->right->access_(i-lsize,delta, n);
                }
            }
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->access_(i);
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            return (*stat)->access_(i);
        }
        throw std::runtime_error("HybridBV::access_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    uint HybridBV::hybridAccess_(uint64_t i) { 
        int64_t delta = 0;
        uint64_t n = 0;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            n = (*dyn)->size;
        }
        uint answ = access_(i,&delta, n);
        if (delta) {
            recompute(i,delta);
        }
        return answ;
    }

    // read bits [i..i+l-1], onto D[j...]
    void HybridBV::read(uint64_t i, uint64_t l, uint64_t *D, uint64_t j, uint *recomp, uint64_t n) { 
        uint64_t lsize;
        int64_t delta;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) { 
            (*dyn)->accesses++;
            if (mustFlatten(n)) {
                delta = 0;
                flatten(&delta); 
                if (delta) *recomp = 1;
            } else {
                lsize = (*dyn)->left->length();
                if (i+l < lsize) {
                    (*dyn)->left->read(i,l,D,j,recomp, n);
                } else if (i>=lsize) {
                    (*dyn)->right->read(i-lsize,l,D,j,recomp, n);
                } else { 
                    (*dyn)->left->read(i,lsize-i,D,j,recomp, n);
                    (*dyn)->right->read(0,l-(lsize-i),D,j+(lsize-i),recomp, n);
                }
                return;
            }
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) { 
            (*leaf)->read(i,l,D,j);
            return;
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            (*stat)->read(i,l,D,j);
        }
    }

    void HybridBV::hybridRead (uint64_t i, uint64_t l, uint64_t *D, uint64_t j) { 
        uint recomp = 0;
        uint64_t n = 0;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            n = (*dyn)->size;
        }
        read(i,l,D,j,&recomp, n);
        if (recomp) {
            rrecompute(i,l);
        }
    }

    // computes rank_1(B,i), zero-based, assumes i is right
    uint64_t HybridBV::rank_(uint64_t i, int64_t *delta, uint64_t n) {
        uint64_t lsize;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            (*dyn)->accesses++;
            if (mustFlatten(n)) {
                flatten(delta); 
            } else {
                lsize = (*dyn)->left->length();
                if (i < lsize) {
                    return (*dyn)->left->rank_(i,delta, n);
                } else {
                return (*dyn)->left->getOnes() + (*dyn)->right->rank_(i-lsize,delta, n);
                }
            }
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->rank_(i);
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            return (*stat)->rank_(i);
        }
        throw std::runtime_error("HybridBV::rank_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    uint64_t HybridBV::hybridRank_(uint64_t i) { 
        int64_t delta = 0;
        uint64_t n = 0;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            n = (*dyn)->size;
        }
        uint64_t answ = rank_(i,&delta, n);
        if (delta) {
            recompute(i,delta);
        }
        return answ;
    }

    // computes select_1(B,j), zero-based, assumes j is right
    uint64_t HybridBV::select1_(uint64_t j, int64_t *delta, uint64_t n) {
        uint64_t lones;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            (*dyn)->accesses++;
            if (mustFlatten(n)) {
                flatten(delta);
            } else { 
                lones = (*dyn)->left->getOnes();
                if (j <= lones) {
                    return (*dyn)->left->select1_(j,delta, n);
                }
                return (*dyn)->left->length() + (*dyn)->right->select1_(j-lones,delta, n);
            }
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->select1_(j);   
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            return (*stat)->select1_(j);
        }
        throw std::runtime_error("HybridBV::select1_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    // computes select_0(B,j), zero-based, assumes j is right
    uint64_t HybridBV::select0_(uint64_t j, int64_t *delta, uint64_t n) { 
        uint64_t lzeros;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            (*dyn)->accesses++;
            if (mustFlatten(n)) {
                flatten(delta); 
            } else { 
                lzeros = (*dyn)->left->length() - (*dyn)->left->getOnes();
                if (j <= lzeros) {
                    return (*dyn)->left->select0_(j,delta, n);
                }
                return (*dyn)->left->length() + (*dyn)->right->select0_(j-lzeros,delta, n);
            }
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->select0_(j);
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            return (*stat)->select0_(j);
        }
        throw std::runtime_error("HybridBV::select0_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    // computes next_1(B,i), zero-based and including i
    // returns -1 if no answer
    int64_t HybridBV::next1 (uint64_t i, int64_t *delta, uint64_t n) { 
        uint64_t lsize;
        int64_t next;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) { 
            if (getOnes() == 0) return -1; // not considered an access!
            (*dyn)->accesses++;
            if (mustFlatten(n)){
                flatten(delta); 
            } else { 
                lsize =(*dyn)->left->length();
                if (i < lsize) {
                    next = (*dyn)->left->next1(i,delta,n);
                    if (next != -1) return next;
                    i = lsize;
                }
                next = (*dyn)->right->next1(i-lsize,delta,n);
                if (next == -1) return -1;
                return lsize + next;
            }
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->next1(i);
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            return (*stat)->next1(i);
        }
        throw std::runtime_error("HybridBV::select0_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    // computes next_0(B,i), zero-based and including i
    // returns -1 if no answer
    int64_t HybridBV::next0(uint64_t i, int64_t *delta, uint64_t n) {
        uint64_t lsize;
        int64_t next;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) { 
            if (getOnes() == length()) return -1; // not an access
            (*dyn)->accesses++;
            if (mustFlatten(n)) {
                flatten(delta);
            } else { 
                lsize = (*dyn)->left->length();
                if (i < lsize) { 
                    next = (*dyn)->left->next0(i,delta,n);
                    if (next != -1) return next;
                    i = lsize;
                }
                next = (*dyn)->right->next0(i-lsize,delta,n);
                if (next == -1) return -1;
                return lsize + next;
            }
        }
        if (auto leaf = std::get_if<LeafBV*>(&bv)) {
            return (*leaf)->next0(i);
        } else if (auto stat = std::get_if<StaticBV*>(&bv)) {
            return (*stat)->next0(i);
        }
        throw std::runtime_error("HybridBV::select0_(): tipo inesperado, se esperaba StaticBV, LeafBV o DynamicBV");
    }

    int64_t HybridBV::hybridNext1(uint64_t i) { 
        int64_t delta = 0;
        uint64_t n = 0;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            n = (*dyn)->size;
        }
        int64_t answ = next1(i,&delta,n);
        if (delta) recompute(i,delta);
        return answ;
    }

    int64_t HybridBV::hybridNext0(uint64_t i) { 
        int64_t delta = 0;
        uint64_t n = 0;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            n = (*dyn)->size;
        }
        int64_t answ = next0(i,&delta,n);
        if (delta) recompute(i,delta);
        return answ;
    }

    uint64_t HybridBV::hybridSelect1_(uint64_t j) { 
        int64_t delta = 0;
        uint64_t n = 0;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            n = (*dyn)->size;
        }
        uint64_t answ = select1_(j,&delta, n);
        if (delta) {
            recompute(answ,delta);
        }
        return answ;
    }

    uint64_t HybridBV::hybridSelect0_(uint64_t j) { 
        int64_t delta = 0;
        uint64_t n = 0;
        if (auto dyn = std::get_if<DynamicBV*>(&bv)) {
            n = (*dyn)->size;
        }
        uint64_t answ = select0_(j,&delta, n);
        if (delta) {
            recompute(answ,delta);
        }
        return answ;
    }

    uint64_t HybridBV::at(uint64_t i) {
        return hybridAccess_(i);
    }

    uint64_t HybridBV::rank(uint64_t i, bool b) {
        uint64_t r1 = hybridRank_(i);

        return b ? r1 : i - r1;
    }

    uint64_t HybridBV::select1(uint64_t i) {
        return hybridSelect1_(i + 1);
    }

    uint64_t HybridBV::select0(uint64_t i) {
        return hybridSelect0_(i + 1);
    }

    uint64_t HybridBV::select(uint64_t i, bool b) {
        return b ? select1(i) : select0(i);
    }

    uint64_t HybridBV::size() {
        return length();
    }

    void HybridBV::insert(uint64_t i, uint v) {
        hybridInsert_(i, v);
    }

    void HybridBV::insert0(uint64_t i) {
        insert(i, 0);
    }

    void HybridBV::insert1(uint64_t i){
        insert(i, 1);
    }

    void HybridBV::push_back(uint i){
        insert(length(), i);
    }

    int HybridBV::remove(uint64_t i) {
        return hybridRemove_(i);
    }

    int HybridBV::set(uint64_t i, bool b) {
        return write_(i, b);
    }
}