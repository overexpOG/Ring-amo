/*
 * bwt.hpp
 * Copyright (C) 2020 Author removed for double-blind evaluation
 * 
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef BWT_T
#define BWT_T

#include "configuration.hpp"

using namespace std;


namespace ring {

    template <class bwt_bit_vector_t = bit_vector,
            class bwt_rank_1_t = typename bit_vector::rank_1_type,
            class bwt_select_1_t = select_support_scan<1>,
            class bwt_select_0_t = select_support_scan<0>>
    class bwt {

    public:
        typedef uint64_t value_type;
        typedef uint64_t size_type;
        typedef sdsl::bit_vector c_type;
        typedef sdsl::rank_support_v<> c_rank_type;
        typedef sdsl::select_support_mcl<1> c_select_1_type;
        typedef sdsl::select_support_mcl<0> c_select_0_type;
        typedef sdsl::wm_int<bwt_bit_vector_t, bwt_rank_1_t, bwt_select_1_t, bwt_select_0_t> bwt_type;

    private:
        bwt_type m_L;
        c_type m_C;
        c_rank_type m_C_rank;
        c_select_1_type m_C_select1;
        c_select_0_type m_C_select0;

        void copy(const bwt &o) {
            m_L = o.m_L;
            m_C = o.m_C;
            m_C_rank = o.m_C_rank;
            m_C_rank.set_vector(&m_C);
            m_C_select1 = o.m_C_select1;
            m_C_select1.set_vector(&m_C);
            m_C_select0 = o.m_C_select0;
            m_C_select0.set_vector(&m_C);
        }

    public:


        bwt() = default;

        bwt(const int_vector<> &L, const vector<uint64_t> &C, uint64_t sigma = 0) {
            //Building the wavelet matrix
            construct_im(m_L, L);
            //Building C and its rank and select structures
            m_C = c_type(C[C.size() - 1] + 1 + C.size(), 0);
            for (uint64_t i = 0; i < C.size(); i++) {
                m_C[C[i] + i] = 1;
            }
            util::init_support(m_C_rank, &m_C);
            util::init_support(m_C_select1, &m_C);
            util::init_support(m_C_select0, &m_C);
        }


        //! Copy constructor
        bwt(const bwt &o) {
            copy(o);
        }

        //! Move constructor
        bwt(bwt &&o) {
            *this = std::move(o);
        }

        //! Copy Operator=
        bwt &operator=(const bwt &o) {
            if (this != &o) {
                copy(o);
            }
            return *this;
        }

        //! Move Operator=
        bwt &operator=(bwt &&o) {
            if (this != &o) {
                m_L = std::move(o.m_L);
                m_C = std::move(o.m_C);
                m_C_rank = std::move(o.m_C_rank);
                m_C_rank.set_vector(&m_C);
                m_C_select1 = std::move(o.m_C_select1);
                m_C_select1.set_vector(&m_C);
                m_C_select0 = std::move(o.m_C_select0);
                m_C_select0.set_vector(&m_C);
            }
            return *this;
        }

        void print_tree()
        {
            uint64_t n = m_L.size() * m_L.max_level;
            std::cout << m_L.max_level << std::endl;
            for (uint64_t i = 0; i < n; i++) {
                std::cout << m_L.tree[i] << " ";
                if ((i + 1) % m_L.size() == 0) std::cout << std::endl;
            }
        }

        void print_C()
        {
            std::cout << "C bitvector" << std::endl;
            for (uint64_t i = 0; i < m_C.size(); i++) {
                std::cout << m_C[i] << " ";
            }
            std::cout << std::endl;
        }

        void statistics() {
            cout << "Not implemented" << endl;
        }

        uint64_t bit_size() {
            cout << "Not implemented" << endl;
            return 1;
        }

        void swap(bwt &o) {
            // m_bp.swap(bp_support.m_bp); use set_vector to set the supported bit_vector
            std::swap(m_L, o.m_L);
            std::swap(m_C, o.m_C);
            sdsl::util::swap_support(m_C_rank, o.m_C_rank, &m_C, &o.m_C);
            sdsl::util::swap_support(m_C_select1, o.m_C_select1, &m_C, &o.m_C);
            sdsl::util::swap_support(m_C_select0, o.m_C_select0, &m_C, &o.m_C);
        }


        //! Serializes the data structure into the given ostream
        size_type serialize(std::ostream &out, sdsl::structure_tree_node *v = nullptr, std::string name = "") const {
            sdsl::structure_tree_node *child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
            size_type written_bytes = 0;
            written_bytes += m_L.serialize(out, child, "L");
            written_bytes += m_C.serialize(out, child, "C");
            written_bytes += m_C_rank.serialize(out, child, "C_rank");
            written_bytes += m_C_select1.serialize(out, child, "C_select1");
            written_bytes += m_C_select0.serialize(out, child, "C_select0");
            sdsl::structure_tree::add_size(child, written_bytes);
            return written_bytes;
        }

        void load(std::istream &in) {
            m_L.load(in);
            m_C.load(in);
            m_C_rank.load(in, &m_C);
            m_C_select1.load(in, &m_C);
            m_C_select0.load(in, &m_C);
        }

        //Operations
        inline size_type get_C(const uint64_t v) const {
            return m_C_select1(v + 1) - v;
        }

        inline uint64_t LF(uint64_t i) {
            uint64_t s = m_L[i];
            return get_C(s) + m_L.rank(i, s) - 1;
        }

        uint64_t nElems(uint64_t val) {
            return get_C(val + 1) - get_C(val);
        }

        pair<uint64_t, uint64_t>
        backward_step(uint64_t left_end, uint64_t right_end, uint64_t value) {
            return {m_L.rank(left_end, value), m_L.rank(right_end + 1, value) - 1};
        }

        inline uint64_t bsearch_C(uint64_t value) {
            return m_C_rank(m_C_select0(value + 1));
        }


        inline uint64_t ranky(uint64_t pos, uint64_t val) {
            return m_L.rank(pos, val);
        }

        inline uint64_t rank(uint64_t pos, uint64_t val) {
            // std::cout << "Calling rank: " << m_L.rank(get_C(pos), val) << std::endl;
            return m_L.rank(get_C(pos), val);
        }

        inline uint64_t select(uint64_t _rank, uint64_t val) {
            return m_L.select(_rank, val);
        }

        inline std::pair<uint64_t, uint64_t> select_next(uint64_t pos, uint64_t val, uint64_t n_elems) {
            return m_L.select_next(get_C(pos), val, n_elems);
        }

        inline uint64_t min_in_range(uint64_t l, uint64_t r) {
            return m_L.range_minimum_query(l, r);
        }

        inline uint64_t range_next_value(uint64_t x, uint64_t l, uint64_t r) {
            return m_L.range_next_value(x, l, r);
        }

        std::vector<uint64_t>
        values_in_range(uint64_t pos_min, uint64_t pos_max) {
            //interval_symbols(L, pos_min, pos_max+1, k, values, r_i, r_j);
            return m_L.all_values_in_range(pos_min, pos_max);
        }

        // backward search for pattern of length 1
        pair<uint64_t, uint64_t> backward_search_1_interval(uint64_t P) const {
            return {get_C(P), get_C(P + 1) - 1};
        }

        // backward search for pattern of length 1
        pair<uint64_t, uint64_t> backward_search_1_rank(uint64_t P, uint64_t S) const {
            return {m_L.rank(get_C(P), S), m_L.rank(get_C(P + 1), S)};
        }

        // backward search for pattern PQ of length 2
        // returns an empty interval if search is unsuccessful
        pair<uint64_t, uint64_t>
        backward_search_2_interval(uint64_t P, pair<uint64_t, uint64_t> &I) const {
            return {get_C(P) + I.first, get_C(P) + I.second - 1};
        }

        pair<uint64_t, uint64_t>
        backward_search_2_rank(uint64_t P, uint64_t S, pair<uint64_t, uint64_t> &I) const {
            uint64_t c = get_C(P);
            return {m_L.rank(c + I.first, S), m_L.rank(c + I.second, S)};
        }

        inline std::pair<uint64_t, uint64_t> inverse_select(uint64_t pos)
        {
            return m_L.inverse_select(pos); 
        }

        inline uint64_t operator[](uint64_t i)
        {
            return m_L[i];
        }

    };

    typedef bwt<> bwt_no_select;
    typedef bwt<bit_vector,
                typename bit_vector::rank_1_type,
                typename bit_vector::select_1_type,
                typename bit_vector::select_0_type> bwt_plain;

    typedef bwt<rrr_vector<15>,
            typename rrr_vector<15>::rank_1_type,
            typename rrr_vector<15>::select_1_type,
            typename rrr_vector<15>::select_0_type> bwt_rrr;
}

#endif
