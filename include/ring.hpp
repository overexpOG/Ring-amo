/*
 * ring.hpp
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

#ifndef RING_HPP
#define RING_HPP

#include <cstdint>
#include "bwt.hpp"
#include "bwt_dyn.hpp"
#include "bwt_interval.hpp"

#include <stdio.h>
#include <stdlib.h>

namespace ring
{

    template <class bwt_so_t = bwt<>, class bwt_p_t = bwt_plain>
    class ring
    {
    public:
        typedef uint64_t size_type;
        typedef uint64_t value_type;
        typedef bwt_so_t bwt_so_type;
        typedef bwt_p_t bwt_p_type;
        typedef std::tuple<uint32_t, uint32_t, uint32_t> spo_triple_type;

    private:
        bwt_so_type m_bwt_s; // POS
        bwt_p_type m_bwt_p;  // OSP
        bwt_so_type m_bwt_o; // SPO

        size_type m_max_s;
        size_type m_max_p;
        size_type m_max_o;
        size_type m_n_triples; // number of triples

        void copy(const ring &o)
        {
            m_bwt_s = o.m_bwt_s;
            m_bwt_p = o.m_bwt_p;
            m_bwt_o = o.m_bwt_o;
            m_max_s = o.m_max_s;
            m_max_p = o.m_max_p;
            m_max_o = o.m_max_o;
            m_n_triples = o.m_n_triples;
        }

    public:
        ring() = default;

        // Assumes the triples have been stored in a vector<spo_triple>
        ring(vector<spo_triple_type> &D)
        {
            uint64_t i, pos_c;
            vector<spo_triple>::iterator it, triple_begin = D.begin(), triple_end = D.end();
            uint64_t U, n = m_n_triples = D.size();

            {
                m_max_p = std::get<1>(D[0]), U = std::get<0>(D[0]);
                if (std::get<2>(D[0]) > U)
                    U = std::get<2>(D[0]);

                for (uint64_t i = 1; i < n; i++)
                {
                    if (std::get<1>(D[i]) > m_max_p)
                        m_max_p = std::get<1>(D[i]);

                    if (std::get<0>(D[i]) > U)
                        U = std::get<0>(D[i]);

                    if (std::get<2>(D[i]) > U)
                        U = std::get<2>(D[i]);
                }
            }
            uint64_t alphabet_SO = U;
            m_max_s = m_max_o = alphabet_SO;

            std::vector<uint32_t> M_O, M_S(alphabet_SO + 1, 0), M_P;

            M_S.shrink_to_fit();

            for (it = triple_begin; it != triple_end; it++)
            {
                M_S[std::get<0>(*it)]++;
            }

            // Sorts the triples lexycographically
            sort(triple_begin, triple_end);

            // First O
            {
                uint64_t i, c;
                vector<uint64_t> new_C_O;
                uint64_t cur_pos = 1;
                new_C_O.push_back(0); // Dummy value
                new_C_O.push_back(cur_pos);
                for (c = 2; c <= alphabet_SO; c++)
                {
                    cur_pos += M_S[c - 1];
                    new_C_O.push_back(cur_pos);
                }
                new_C_O.push_back(n + 1);
                new_C_O.shrink_to_fit();

                M_S.clear();
                M_S.shrink_to_fit();

                int_vector<> new_O(n + 1);
                new_O[0] = 0;
                for (i = 1; i <= n; i++)
                    new_O[i] = std::get<2>(D[i - 1]);

                util::bit_compress(new_O);
                // builds the WT for BWT(O)
                m_bwt_o = bwt_so_type(new_O, new_C_O, alphabet_SO);
            }

            M_O.resize(alphabet_SO + 1, 0);
            M_O.shrink_to_fit();

            for (it = triple_begin, i = 0; i < n; i++, it++)
                M_O[std::get<2>(*it)]++;

            stable_sort(D.begin(), D.end(), [](const spo_triple &a, const spo_triple &b)
                        { return std::get<2>(a) < std::get<2>(b); });
            // Now P
            {
                uint64_t c, i;
                vector<uint64_t> new_C_P;

                uint64_t cur_pos = 1;
                new_C_P.push_back(0); // Dummy value
                new_C_P.push_back(cur_pos);
                for (c = 2; c <= alphabet_SO; c++)
                {
                    cur_pos += M_O[c - 1];
                    new_C_P.push_back(cur_pos);
                }
                new_C_P.push_back(n + 1);
                new_C_P.shrink_to_fit();

                M_O.clear();

                int_vector<> new_P(n + 1);
                new_P[0] = 0;
                for (i = 1; i <= n; i++)
                    new_P[i] = std::get<1>(D[i - 1]);

                util::bit_compress(new_P);
                m_bwt_p = bwt_p_type(new_P, new_C_P, m_max_p);
            }

            M_P.resize(m_max_p + 1, 0);
            M_P.shrink_to_fit();

            for (it = triple_begin, i = 0; i < n; i++, it++)
                M_P[std::get<1>(*it)]++;

            stable_sort(D.begin(), D.end(), [](const spo_triple &a, const spo_triple &b)
                        { return std::get<1>(a) < std::get<1>(b); });
            // Builds BWT_S
            {
                uint64_t i, c;
                vector<uint64_t> new_C_S;

                uint64_t cur_pos = 1;
                new_C_S.push_back(0); // Dummy value
                new_C_S.push_back(cur_pos);
                for (c = 2; c <= m_max_p; c++)
                {
                    cur_pos += M_P[c - 1];
                    new_C_S.push_back(cur_pos);
                }
                new_C_S.push_back(n + 1);
                new_C_S.shrink_to_fit();

                M_P.clear();

                int_vector<> new_S(n + 1);
                new_S[0] = 0;
                for (i = 1; i <= n; i++)
                    new_S[i] = std::get<0>(D[i - 1]);
                util::bit_compress(new_S);
                m_bwt_s = bwt_so_type(new_S, new_C_S, alphabet_SO);
            }

            // cout << "-- Index constructed successfully" << endl; fflush(stdout);
        };

        //! Copy constructor
        ring(const ring &o)
        {
            copy(o);
        }

        //! Move constructor
        ring(ring &&o)
        {
            *this = std::move(o);
        }

        //! Copy Operator=
        ring &operator=(const ring &o)
        {
            if (this != &o)
            {
                copy(o);
            }
            return *this;
        }

        //! Move Operator=
        ring &operator=(ring &&o)
        {
            if (this != &o)
            {
                m_bwt_s = std::move(o.m_bwt_s);
                m_bwt_p = std::move(o.m_bwt_p);
                m_bwt_o = std::move(o.m_bwt_o);
                m_max_s = o.m_max_s;
                m_max_p = o.m_max_p;
                m_max_o = o.m_max_o;
                m_n_triples = o.m_n_triples;
            }
            return *this;
        }

        void swap(ring &o)
        {
            // m_bp.swap(bp_support.m_bp); use set_vector to set the supported bit_vector
            std::swap(m_bwt_s, o.m_bwt_s);
            std::swap(m_bwt_p, o.m_bwt_p);
            std::swap(m_bwt_o, o.m_bwt_o);
            std::swap(m_max_s, o.m_max_s);
            std::swap(m_max_p, o.m_max_p);
            std::swap(m_max_o, o.m_max_o);
            std::swap(m_n_triples, o.m_n_triples);
        }

        //! Serializes the data structure into the given ostream
        size_type serialize(std::ostream &out, sdsl::structure_tree_node *v = nullptr, std::string name = "")
        {
            sdsl::structure_tree_node *child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
            size_type written_bytes = 0;
            written_bytes += m_bwt_s.serialize(out, child, "bwt_s");
            written_bytes += m_bwt_p.serialize(out, child, "bwt_p");
            written_bytes += m_bwt_o.serialize(out, child, "bwt_o");
            written_bytes += sdsl::write_member(m_max_s, out, child, "max_s");
            written_bytes += sdsl::write_member(m_max_p, out, child, "max_p");
            written_bytes += sdsl::write_member(m_max_o, out, child, "max_o");
            written_bytes += sdsl::write_member(m_n_triples, out, child, "n_triples");
            sdsl::structure_tree::add_size(child, written_bytes);
            return written_bytes;
        }

        void load(std::istream &in)
        {
            m_bwt_s.load(in);
            m_bwt_p.load(in);
            m_bwt_o.load(in);
            sdsl::read_member(m_max_s, in);
            sdsl::read_member(m_max_p, in);
            sdsl::read_member(m_max_o, in);
            sdsl::read_member(m_n_triples, in);
        }

        uint64_t range_next_value_P(uint64_t x, uint64_t l, uint64_t r)
        {
            return m_bwt_p.range_next_value(x, l, r);
        }

        uint64_t range_next_value_S(uint64_t x, uint64_t l, uint64_t r)
        {
            return m_bwt_s.range_next_value(x, l, r);
        }

        uint64_t range_next_value_O(uint64_t x, uint64_t l, uint64_t r)
        {
            return m_bwt_o.range_next_value(x, l, r);
        }

        bwt_so_type get_S()
        {
            return m_bwt_s;
        }

        bwt_so_type get_O()
        {
            return m_bwt_o;
        }

        bwt_p_type get_P()
        {
            return m_bwt_p;
        }

        // Given a Suffix returns its range in BWT O
        pair<uint64_t, uint64_t> init_S(uint64_t S)
        {
            return m_bwt_o.backward_search_1_interval(S);
        }

        // Given a Predicate returns its range in BWT S
        pair<uint64_t, uint64_t> init_P(uint64_t P)
        {
            return m_bwt_s.backward_search_1_interval(P);
        }

        // Given an Object returns its range in BWT P
        pair<uint64_t, uint64_t> init_O(uint64_t O)
        {
            return m_bwt_p.backward_search_1_interval(O);
        }

        void print()
        {
            std::cout << "Number of triples: " << m_n_triples << std::endl;
            std::cout << "SPO WM" << std::endl;
            m_bwt_o.print_tree();
            m_bwt_o.print_C();

            std::cout << "OSP WM" << std::endl;
            m_bwt_p.print_tree();
            m_bwt_p.print_C();

            std::cout << "POS WM" << std::endl;
            m_bwt_s.print_tree();
            m_bwt_s.print_C();
        }



        // POS -> SPO
        pair<uint64_t, uint64_t> init_SP(uint64_t S, uint64_t P)
        {
            auto I = m_bwt_s.backward_search_1_rank(P, S);   // POS
            return m_bwt_o.backward_search_2_interval(S, I); // SPO
        }

        // SPO -> OSP
        pair<uint64_t, uint64_t> init_SO(uint64_t S, uint64_t O)
        {
            auto I = m_bwt_o.backward_search_1_rank(S, O);   // SPO
            return m_bwt_p.backward_search_2_interval(O, I); // OSP
            // return {I.first + 2 * m_n_triples, I.second + 2 * m_n_triples};
        }

        // OSP -> POS
        pair<uint64_t, uint64_t> init_PO(uint64_t P, uint64_t O)
        {
            auto I = m_bwt_p.backward_search_1_rank(O, P);   // OSP
            return m_bwt_s.backward_search_2_interval(P, I); // POS
            // return {I.first + m_n_triples, I.second + m_n_triples};
        }

        // OSP -> POS -> SPO
        pair<uint64_t, uint64_t> init_SPO(uint64_t S, uint64_t P, uint64_t O)
        {
            auto I = m_bwt_p.backward_search_1_rank(O, P);   // OSP
            I = m_bwt_s.backward_search_2_rank(P, S, I);     // POS
            return m_bwt_o.backward_search_2_interval(S, I); // SPO
        }

        /**********************************/
        // Functions for PSO
        //

        bwt_interval open_PSO()
        {
            // return bwt_interval(2 * m_n_triples + 1, 3 * m_n_triples);
            return bwt_interval(1, m_n_triples);
        }

        /**********************************/
        // P->S  (simulates going down in the trie)
        // Returns an interval within m_bwt_o
        bwt_interval down_P_S(bwt_interval &p_int, uint64_t s)
        {
            auto I = m_bwt_s.backward_step(p_int.left(), p_int.right(), s);
            uint64_t c = m_bwt_o.get_C(s);
            return bwt_interval(I.first + c, I.second + c);
        }

        uint64_t min_O_in_S(bwt_interval &I)
        {
            return I.begin(m_bwt_o);
        }

        uint64_t next_O_in_S(bwt_interval &I, uint64_t O)
        {
            if (O > m_max_o)
                return 0;

            uint64_t nextv = I.next_value(O, m_bwt_o);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bool there_are_O_in_S(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        uint64_t min_O_in_PS(bwt_interval &I)
        {
            return I.begin(m_bwt_o);
        }

        uint64_t next_O_in_PS(bwt_interval &I, uint64_t O)
        {
            if (O > m_max_o)
                return 0;

            uint64_t nextv = I.next_value(O, m_bwt_o);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bool there_are_O_in_PS(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        std::vector<uint64_t>
        all_O_in_range(bwt_interval &I)
        {
            return m_bwt_o.values_in_range(I.left(), I.right());
        }

        /**********************************/
        // Functions for OPS
        //

        bwt_interval open_OPS()
        {
            return bwt_interval(1, m_n_triples);
        }

        /**********************************/
        // O->P  (simulates going down in the trie)
        // Returns an interval within m_bwt_s
        bwt_interval down_O_P(bwt_interval &o_int, uint64_t p)
        {
            auto I = m_bwt_p.backward_step(o_int.left(), o_int.right(), p);
            uint64_t c = m_bwt_s.get_C(p);
            return bwt_interval(I.first + c, I.second + c);
        }

        uint64_t min_S_in_OP(bwt_interval &I)
        {
            return I.begin(m_bwt_s);
        }

        uint64_t next_S_in_OP(bwt_interval &I, uint64_t s_value)
        {
            if (s_value > m_max_s)
                return 0;

            uint64_t nextv = I.next_value(s_value, m_bwt_s);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bool there_are_S_in_OP(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        uint64_t min_S_in_P(bwt_interval &I)
        {
            return I.begin(m_bwt_s);
        }

        uint64_t next_S_in_P(bwt_interval &I, uint64_t s_value)
        {
            if (s_value > m_max_s)
                return 0;

            uint64_t nextv = I.next_value(s_value, m_bwt_s);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bool there_are_S_in_P(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        std::vector<uint64_t>
        all_S_in_range(bwt_interval &I)
        {
            return m_bwt_s.values_in_range(I.left(), I.right());
        }

        /**********************************/
        // Function for SOP
        //

        bwt_interval open_SOP()
        {
            return bwt_interval(1, m_n_triples);
        }

        /**********************************/
        // S->O  (simulates going down in the trie)
        // Returns an interval within m_bwt_p
        bwt_interval down_S_O(bwt_interval &s_int, uint64_t o)
        {
            pair<uint64_t, uint64_t> I = m_bwt_o.backward_step(s_int.left(), s_int.right(), o);
            uint64_t c = m_bwt_p.get_C(o);
            return bwt_interval(I.first + c, I.second + c);
        }

        uint64_t min_P_in_SO(bwt_interval &I)
        {
            return I.begin(m_bwt_p);
        }

        uint64_t next_P_in_SO(bwt_interval &I, uint64_t p_value)
        {
            if (p_value > m_max_p)
                return 0;

            uint64_t nextv = I.next_value(p_value, m_bwt_p);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bool there_are_P_in_SO(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        uint64_t min_P_in_O(bwt_interval &I)
        {
            return I.begin(m_bwt_p);
        }

        uint64_t next_P_in_O(bwt_interval &I, uint64_t p_value)
        {
            if (p_value > m_max_p)
                return 0;

            uint64_t nextv = I.next_value(p_value, m_bwt_p);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bool there_are_P_in_O(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        std::vector<uint64_t>
        all_P_in_range(bwt_interval &I)
        {
            return m_bwt_p.values_in_range(I.left(), I.right());
        }

        /**********************************/
        // Functions for SPO
        //
        bwt_interval open_SPO()
        {
            return bwt_interval(1, m_n_triples);
        }

        uint64_t min_S(bwt_interval &I)
        {
            return I.begin(m_bwt_s);
        }

        uint64_t next_S(bwt_interval &I, uint64_t s_value)
        {
            if (s_value > m_max_s)
                return 0;

            return I.next_value(s_value, m_bwt_s);
        }

        bwt_interval down_S(uint64_t s_value)
        {
            pair<uint64_t, uint64_t> i = init_S(s_value);
            return bwt_interval(i.first, i.second);
        }

        // S->P  (simulates going down in the trie, for the order SPO)
        // Returns an interval within m_bwt_p
        bwt_interval down_S_P(bwt_interval &s_int, uint64_t s_value, uint64_t p_value)
        {
            std::pair<uint64_t, uint64_t> q = s_int.get_stored_values();
            uint64_t b = q.first;
            if (q.first == (uint64_t)-1)
            {
                q = m_bwt_s.select_next(p_value, s_value, m_bwt_o.nElems(s_value));
                b = m_bwt_s.bsearch_C(q.first) - 1;
            }
            uint64_t nE = m_bwt_s.rank(b + 1, s_value) - m_bwt_s.rank(b, s_value);
            uint64_t start = q.second;

            return bwt_interval(s_int.left() + start, s_int.left() + start + nE - 1);
        }

        uint64_t min_P_in_S(bwt_interval &I, uint64_t s_value);

        uint64_t next_P_in_S(bwt_interval &I, uint64_t s_value, uint64_t p_value);

        uint64_t min_O_in_SP(bwt_interval &I)
        {
            return I.begin(m_bwt_o);
        }

        uint64_t next_O_in_SP(bwt_interval &I, uint64_t O)
        {
            if (O > m_max_o)
                return 0;

            uint64_t next_v = I.next_value(O, m_bwt_o);
            if (next_v == 0)
                return 0;
            else
                return next_v;
        }

        bool there_are_O_in_SP(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        /**********************************/
        // Functions for POS
        //

        bwt_interval open_POS()
        {
            return bwt_interval(1, m_n_triples);
        }

        uint64_t min_P(bwt_interval &I)
        {
            // bwt_interval I_aux(I.left() - 2 * m_n_triples, I.right() - 2 * m_n_triples);
            // return I_aux.begin(m_bwt_p);
            return I.begin(m_bwt_p);
        }

        uint64_t next_P(bwt_interval &I, uint64_t p_value)
        {
            if (p_value > m_max_p)
                return 0;

            // bwt_interval I_aux(I.left() - 2 * m_n_triples, I.right() - 2 * m_n_triples);
            // uint64_t nextv = I_aux.next_value(p_value, m_bwt_p);
            uint64_t nextv = I.next_value(p_value, m_bwt_p);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bwt_interval down_P(uint64_t p_value)
        {
            pair<uint64_t, uint64_t> i = init_P(p_value);
            return bwt_interval(i.first, i.second);
        }

        // P->O  (simulates going down in the trie, for the order POS)
        // Returns an interval within m_bwt_p
        bwt_interval down_P_O(bwt_interval &p_int, uint64_t p_value, uint64_t o_value)
        {
            std::pair<uint64_t, uint64_t> q = p_int.get_stored_values();
            uint64_t b = q.first;
            if (q.first == (uint64_t)-1)
            {
                q = m_bwt_p.select_next(o_value, p_value, m_bwt_s.nElems(p_value));
                b = m_bwt_p.bsearch_C(q.first) - 1;
            }
            uint64_t nE = m_bwt_p.rank(b + 1, p_value) - m_bwt_p.rank(b, p_value);
            uint64_t start = q.second;

            return bwt_interval(p_int.left() + start, p_int.left() + start + nE - 1);
        }

        uint64_t min_O_in_P(bwt_interval &p_int, uint64_t p_value)
        {
            std::pair<uint64_t, uint64_t> q;
            q = m_bwt_p.select_next(1, p_value, m_bwt_s.nElems(p_value));
            uint64_t b = m_bwt_p.bsearch_C(q.first) - 1;
            p_int.set_stored_values(b, q.second);
            return b;
        }

        uint64_t next_O_in_P(bwt_interval &I, uint64_t p_value, uint64_t o_value)
        {
            if (o_value > m_max_o)
                return 0;

            std::pair<uint64_t, uint64_t> q;
            q = m_bwt_p.select_next(o_value, p_value, m_bwt_s.nElems(p_value));
            if (q.first == 0 && q.second == 0)
                return 0;

            uint64_t b = m_bwt_p.bsearch_C(q.first) - 1;
            I.set_stored_values(b, q.second);
            return b;
        }

        uint64_t min_S_in_PO(bwt_interval &I)
        {
            // bwt_interval I_aux = bwt_interval(I.left() - m_n_triples, I.right() - m_n_triples);
            // return I_aux.begin(m_bwt_s);
            return I.begin(m_bwt_s);
        }

        uint64_t next_S_in_PO(bwt_interval &I, uint64_t s_value)
        {
            if (s_value > m_max_s)
                return 0;

            // bwt_interval I_aux = bwt_interval(I.left() - m_n_triples, I.right() - m_n_triples);
            // return I_aux.next_value(s_value, m_bwt_s);
            return I.next_value(s_value, m_bwt_s);
        }

        bool there_are_S_in_PO(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }

        /**********************************/
        // Functions for OSP
        //

        bwt_interval open_OSP()
        {
            return bwt_interval(1, m_n_triples);
        }

        uint64_t min_O(bwt_interval &I)
        {
            return I.begin(m_bwt_o);
        }

        uint64_t next_O(bwt_interval &I, uint64_t o_value)
        {
            if (o_value > m_max_o)
                return 0;

            uint64_t nextv = I.next_value(o_value, m_bwt_o);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bwt_interval down_O(uint64_t o_value)
        {
            pair<uint64_t, uint64_t> i = init_O(o_value);
            return bwt_interval(i.first, i.second);
        }

        // P->O  (simulates going down in the trie, for the order OSP)
        // Returns an interval within m_bwt_p
        bwt_interval down_O_S(bwt_interval &o_int, uint64_t o_value, uint64_t s_value)
        {
            std::pair<uint64_t, uint64_t> q = o_int.get_stored_values();
            uint64_t b = q.first;
            if (q.first == (uint64_t)-1)
            {
                q = m_bwt_o.select_next(s_value, o_value, m_bwt_p.nElems(o_value));
                b = m_bwt_o.bsearch_C(q.first) - 1;
            }
            uint64_t nE = m_bwt_o.rank(b + 1, o_value) - m_bwt_o.rank(b, o_value);
            uint64_t start = q.second;

            return bwt_interval(o_int.left() + start, o_int.left() + start + nE - 1);
        }

        uint64_t min_S_in_O(bwt_interval &o_int, uint64_t o_value);

        uint64_t next_S_in_O(bwt_interval &I, uint64_t o_value, uint64_t s_value);

        void insert(spo_triple triple);

        spo_valid_triple remove_edge_and_check(spo_triple triple);

        void remove_edge(spo_triple triple);

        uint64_t remove_node(uint64_t v);

        uint64_t remove_node_with_check(uint64_t x, std::vector<uint64_t> &so_removed, std::vector<uint64_t> &p_removed);

        uint64_t bit_size();

        uint64_t min_P_in_OS(bwt_interval &I)
        {
            return I.begin(m_bwt_p);
        }

        uint64_t next_P_in_OS(bwt_interval &I, uint64_t p_value)
        {
            if (p_value > m_max_p)
                return 0;

            uint64_t nextv = I.next_value(p_value, m_bwt_p);
            if (nextv == 0)
                return 0;
            else
                return nextv;
        }

        bool there_are_P_in_OS(bwt_interval &I)
        {
            return I.get_cur_value() != I.end();
        }
    };

    template <class bwt_so_t, class bwt_sp_t> // Select in BWT
    uint64_t ring<bwt_so_t, bwt_sp_t>::min_P_in_S(bwt_interval &I, uint64_t s_value)
    {
        std::pair<uint64_t, uint64_t> q;
        q = m_bwt_s.select_next(1, s_value, m_bwt_o.nElems(s_value));
        uint64_t b = m_bwt_s.bsearch_C(q.first) - 1;
        I.set_stored_values(b, q.second);
        return b;
    }

    template <> // No select in BWT
    uint64_t ring<bwt<>>::min_P_in_S(bwt_interval &I, uint64_t s_value)
    {
        uint64_t s_aux = I.left();
        std::pair<uint64_t, uint64_t> o_r = m_bwt_o.inverse_select(s_aux);
        uint64_t p = m_bwt_p[m_bwt_p.get_C(o_r.second) + o_r.first];
        I.set_stored_values(p, 0);
        return p;
    }

    template <> // No select in BWT
    uint64_t ring<bwt_rrr>::min_P_in_S(bwt_interval &I, uint64_t s_value)
    {
        uint64_t s_aux = I.left();
        std::pair<uint64_t, uint64_t> o_r = m_bwt_o.inverse_select(s_aux);
        uint64_t p = m_bwt_p[m_bwt_p.get_C(o_r.second) + o_r.first];
        I.set_stored_values(p, 0);
        return p;
    }

    template <class bwt_so_t, class bwt_sp_t> // Select in BWT
    uint64_t ring<bwt_so_t, bwt_sp_t>::next_P_in_S(bwt_interval &I, uint64_t s_value, uint64_t p_value)
    {
        if (p_value > m_max_p)
            return 0;

        std::pair<uint64_t, uint64_t> q;
        q = m_bwt_s.select_next(p_value, s_value, m_bwt_o.nElems(s_value));
        if (q.first == 0 && q.second == 0)
        {
            return 0;
        }

        uint64_t b = m_bwt_s.bsearch_C(q.first) - 1;
        I.set_stored_values(b, q.second);
        return b;
    }

    template <> // NO Select in BWT
    uint64_t ring<bwt<>>::next_P_in_S(bwt_interval &I, uint64_t s_value, uint64_t p_value)
    {
        if (p_value > m_max_p)
            return 0;

        uint64_t nValues = I.right() - I.left() + 1;
        uint64_t r_aux = m_bwt_s.rank(p_value, s_value);
        if (r_aux >= nValues)
            return 0;
        uint64_t p_aux = I.left();
        std::pair<uint64_t, uint64_t> o_r = m_bwt_o.inverse_select(p_aux + r_aux);
        uint64_t p = m_bwt_p[m_bwt_p.get_C(o_r.second) + o_r.first];
        I.set_stored_values(p, r_aux);
        return p;
    }

    template <> // NO Select in BWT
    uint64_t ring<bwt_rrr>::next_P_in_S(bwt_interval &I, uint64_t s_value, uint64_t p_value)
    {
        if (p_value > m_max_p)
            return 0;

        uint64_t nValues = I.right() - I.left() + 1;
        uint64_t r_aux = m_bwt_s.rank(p_value, s_value);
        if (r_aux >= nValues)
            return 0;
        uint64_t p_aux = I.left();
        std::pair<uint64_t, uint64_t> o_r = m_bwt_o.inverse_select(p_aux + r_aux);
        uint64_t p = m_bwt_p[m_bwt_p.get_C(o_r.second) + o_r.first];
        I.set_stored_values(p, r_aux);
        return p;
    }

    template <class bwt_so_t, class bwt_sp_t> // Select in BWT
    uint64_t ring<bwt_so_t, bwt_sp_t>::min_S_in_O(bwt_interval &o_int, uint64_t o_value)
    {
        std::pair<uint64_t, uint64_t> q;
        q = m_bwt_o.select_next(1, o_value, m_bwt_p.nElems(o_value));
        uint64_t b = m_bwt_o.bsearch_C(q.first) - 1;
        o_int.set_stored_values(b, q.second);
        return b;
    }

    template <> // NO Select in BWT
    uint64_t ring<bwt<>>::min_S_in_O(bwt_interval &o_int, uint64_t o_value)
    {
        uint64_t o_aux = o_int.left();
        std::pair<uint64_t, uint64_t> p_r = m_bwt_p.inverse_select(o_aux);
        uint64_t s = m_bwt_s[m_bwt_s.get_C(p_r.second /*s_value*/) + p_r.first /*r*/];
        o_int.set_stored_values(s, 0);
        return s;
    }

    template <> // NO Select in BWT
    uint64_t ring<bwt_rrr>::min_S_in_O(bwt_interval &o_int, uint64_t o_value)
    {
        uint64_t o_aux = o_int.left();
        std::pair<uint64_t, uint64_t> p_r = m_bwt_p.inverse_select(o_aux);
        uint64_t s = m_bwt_s[m_bwt_s.get_C(p_r.second /*s_value*/) + p_r.first /*r*/];
        o_int.set_stored_values(s, 0);
        return s;
    }

    template <class bwt_so_t, class bwt_sp_t> // Select in BWT
    uint64_t ring<bwt_so_t, bwt_sp_t>::next_S_in_O(bwt_interval &I, uint64_t o_value, uint64_t s_value)
    {
        if (s_value > m_max_s)
            return 0;

        std::pair<uint64_t, uint64_t> q;
        q = m_bwt_o.select_next(s_value, o_value, m_bwt_p.nElems(o_value));
        if (q.first == 0 && q.second == 0)
            return 0;

        uint64_t b = m_bwt_o.bsearch_C(q.first) - 1;
        I.set_stored_values(b, q.second);
        return b;
    }

    template <> // NO Select in BWT
    uint64_t ring<bwt<>>::next_S_in_O(bwt_interval &I, uint64_t o_value, uint64_t s_value)
    {
        if (s_value > m_max_s)
            return 0;

        uint64_t nValues = I.right() - I.left() + 1;
        uint64_t r_aux = m_bwt_o.rank(s_value, o_value);
        if (r_aux >= nValues)
            return 0;
        uint64_t o_aux = I.left();
        std::pair<uint64_t, uint64_t> p_r = m_bwt_p.inverse_select(o_aux + r_aux);
        uint64_t s = m_bwt_s[m_bwt_s.get_C(p_r.second) + p_r.first];
        I.set_stored_values(s, r_aux);
        return s;
    }

    template <> // NO Select in BWT
    uint64_t ring<bwt_rrr>::next_S_in_O(bwt_interval &I, uint64_t o_value, uint64_t s_value)
    {
        if (s_value > m_max_s)
            return 0;

        uint64_t nValues = I.right() - I.left() + 1;
        uint64_t r_aux = m_bwt_o.rank(s_value, o_value);
        if (r_aux >= nValues)
            return 0;
        uint64_t o_aux = I.left();
        std::pair<uint64_t, uint64_t> p_r = m_bwt_p.inverse_select(o_aux + r_aux);
        uint64_t s = m_bwt_s[m_bwt_s.get_C(p_r.second) + p_r.first];
        I.set_stored_values(s, r_aux);
        return s;
    }

     /**
     * @brief Insert a triple in the ring. Keeps the sorting
     *        and updates the bitvectors in the wavelet trees
     * If the triple exists then it doesn't insert it
     *
     * @tparam
     * @param triple The triple being inserted
     */
    template <class bwt_so_t, class bwt_p_t>
    void ring<bwt_so_t, bwt_p_t>::insert(spo_triple triple)
    {
        uint64_t s = get<0>(triple);
        uint64_t p = get<1>(triple);
        uint64_t o = get<2>(triple);

        uint64_t low = 0, high = 0;

        // Update the alphabet size if the symbols are new
        if (s > m_bwt_s.alphabet_size())
        {
            m_bwt_o.push_back_C(1);
            m_bwt_p.push_back_C(1);
            m_bwt_o.increment_alphabet();
            m_bwt_s.increment_alphabet();
        }

        if (p > m_bwt_p.alphabet_size())
        {
            m_bwt_s.push_back_C(1);
            m_bwt_p.increment_alphabet();
        }

        if (o > m_bwt_o.alphabet_size())
        {
            m_bwt_p.push_back_C(1);
            m_bwt_o.push_back_C(1);
            m_bwt_o.increment_alphabet();
            m_bwt_s.increment_alphabet();
        }

        // Insert in the wavelet trees
        low = m_bwt_s.get_C(p);
        high = m_bwt_s.get_C(p + 1) - 1;
        uint64_t insert_index = 0;

        // Inserting in S first
        if (low - 1 == high)
        {
            insert_index = low;
            m_bwt_s.insert_WT(insert_index, s);
            m_bwt_o.insert_C(m_bwt_o.select_C(s + 1), 0);
            insert_index = m_bwt_o.get_C(s) + m_bwt_s.ranky(insert_index, s);
            m_bwt_o.insert_WT(insert_index, o);
            m_bwt_p.insert_C(m_bwt_p.select_C(o + 1), 0);
            insert_index = m_bwt_p.get_C(o) + m_bwt_o.ranky(insert_index, o);
            m_bwt_p.insert_WT(insert_index, p);
            m_bwt_s.insert_C(m_bwt_s.select_C(p + 1), 0);
            m_n_triples++;

            return;
        }

        low = m_bwt_o.get_C(s) + m_bwt_s.ranky(low, s);
        high = m_bwt_o.get_C(s) - 1 + m_bwt_s.ranky(high + 1, s);

        // Inserting in O first
        if (low - 1 == high)
        {
            insert_index = low;
            m_bwt_o.insert_WT(insert_index, o); // SPO
            m_bwt_p.insert_C(m_bwt_p.select_C(o + 1), 0);
            insert_index = m_bwt_p.get_C(o) + m_bwt_o.ranky(insert_index, o);
            m_bwt_p.insert_WT(insert_index, p); // OSP
            m_bwt_s.insert_C(m_bwt_s.select_C(p + 1), 0);
            insert_index = m_bwt_s.get_C(p) + m_bwt_p.ranky(insert_index, p);
            m_bwt_s.insert_WT(insert_index, s); // POS
            m_bwt_o.insert_C(m_bwt_o.select_C(s + 1), 0);
            m_n_triples++;

            return;
        }

        low = m_bwt_p.get_C(o) + m_bwt_o.ranky(low, o);
        high = m_bwt_p.get_C(o) - 1 + m_bwt_o.ranky(high + 1, o);

        // Inserting in P
        if (low - 1 == high)
        {
            insert_index = low;
            m_bwt_p.insert_WT(insert_index, p);
            m_bwt_s.insert_C(m_bwt_s.select_C(p + 1), 0);
            insert_index = m_bwt_s.get_C(p) + m_bwt_p.ranky(insert_index, p);
            m_bwt_s.insert_WT(insert_index, s);
            m_bwt_o.insert_C(m_bwt_o.select_C(s + 1), 0);
            insert_index = m_bwt_o.get_C(s) + m_bwt_s.ranky(insert_index, s);
            m_bwt_o.insert_WT(insert_index, o);
            m_bwt_p.insert_C(m_bwt_p.select_C(o + 1), 0);
            m_n_triples++;

            return;
        }
    }

    /**
     * @brief remove a triple (edge) from the ring. Keeps the sorting
     *        and updates the bitvectors in the wavelet trees
     * At the end of the removal checks if the values are still being used.
     *
     * @tparam
     * @param triple The triple being removed
     *
     * @return A triple of booleans representing if the values are still in use
     */
    template <class bwt_so_t, class bwt_p_t>
    spo_valid_triple ring<bwt_so_t, bwt_p_t>::remove_edge_and_check(spo_triple triple)
    {
        uint64_t s = get<0>(triple);
        uint64_t p = get<1>(triple);
        uint64_t o = get<2>(triple);

        uint64_t low = 0, high = 0;

        // Find triple
        low = m_bwt_s.get_C(p);
        high = m_bwt_s.get_C(p + 1) - 1;

        // Deleting in S first
        if (low == high)
        {
            uint64_t o_remove_index = m_bwt_o.get_C(s) + m_bwt_s.ranky(low, s);
            uint64_t p_remove_index = m_bwt_p.get_C(o) + m_bwt_o.ranky(o_remove_index, o);

            m_bwt_s.remove_WT(low);
            m_bwt_o.remove_WT(o_remove_index);
            m_bwt_p.remove_WT(p_remove_index);

            // Update the bitvectors
            m_bwt_o.remove_C(m_bwt_o.select_C(s + 1) - 1);
            m_bwt_s.remove_C(m_bwt_s.select_C(p + 1) - 1);
            m_bwt_p.remove_C(m_bwt_p.select_C(o + 1) - 1);

            // Check if the elements s,p,o are still in use
            bool s_is_used = m_bwt_o.nElems(s);
            bool p_is_used = m_bwt_s.nElems(p);
            bool o_is_used = m_bwt_p.nElems(o);

            m_n_triples--;

            return {s_is_used, p_is_used, o_is_used};
        }

        low = m_bwt_o.get_C(s) - 1 + m_bwt_s.ranky(low, s) + 1;
        high = m_bwt_o.get_C(s) - 1 + m_bwt_s.ranky(high + 1, s);


        // Deleting in O first
        if (low == high)
        {
            uint64_t p_remove_index = m_bwt_p.get_C(o) + m_bwt_o.ranky(low, o);
            uint64_t s_remove_index = m_bwt_s.get_C(p) + m_bwt_p.ranky(p_remove_index, p);

            m_bwt_o.remove_WT(low);
            m_bwt_p.remove_WT(p_remove_index);
            m_bwt_s.remove_WT(s_remove_index);

            // Update the bitvectors
            m_bwt_o.remove_C(m_bwt_o.select_C(s + 1) - 1);
            m_bwt_s.remove_C(m_bwt_s.select_C(p + 1) - 1);
            m_bwt_p.remove_C(m_bwt_p.select_C(o + 1) - 1);

            // Check if the elements s,p,o are still in use
            bool s_is_used = m_bwt_o.nElems(s) || m_bwt_p.nElems(s);
            bool p_is_used = m_bwt_s.nElems(p);
            bool o_is_used = m_bwt_p.nElems(o) || m_bwt_o.nElems(o);

            m_n_triples--;

            return {s_is_used, p_is_used, o_is_used};
        }

        low = m_bwt_p.get_C(o) - 1 + m_bwt_o.ranky(low, o) + 1;
        high = m_bwt_p.get_C(o) - 1 + m_bwt_o.ranky(high + 1, o);


        // Deleting in P
        if (low <= high)
        {
            uint64_t s_remove_index = m_bwt_s.get_C(p) + m_bwt_p.ranky(low, p);
            uint64_t o_remove_index = m_bwt_o.get_C(s) + m_bwt_s.ranky(s_remove_index, s);

            m_bwt_p.remove_WT(low);
            m_bwt_s.remove_WT(s_remove_index);
            m_bwt_o.remove_WT(o_remove_index);

            // Update the bitvectors
            m_bwt_o.remove_C(m_bwt_o.select_C(s + 1) - 1);
            m_bwt_s.remove_C(m_bwt_s.select_C(p + 1) - 1);
            m_bwt_p.remove_C(m_bwt_p.select_C(o + 1) - 1);

            // Check if the elements s,p,o are still in use
            bool s_is_used = m_bwt_o.nElems(s);
            bool p_is_used = m_bwt_s.nElems(p);
            bool o_is_used = m_bwt_p.nElems(o);

            m_n_triples--;

            return {s_is_used, p_is_used, o_is_used};
        }
        else
        {
            throw std::invalid_argument("The given triple doesnt exist in the graph");
        }
    }

    /**
     * @brief remove a triple (edge) from the ring. Keeps the sorting
     *        and updates the bitvectors in the wavelet trees
     *
     * @tparam
     * @param triple The triple being removed
     */
    template <class bwt_so_t, class bwt_p_t>
    void ring<bwt_so_t, bwt_p_t>::remove_edge(spo_triple triple)
    {
        uint64_t s = get<0>(triple);
        uint64_t p = get<1>(triple);
        uint64_t o = get<2>(triple);

        uint64_t low = 0, high = 0;

        // Find triple
        low = m_bwt_s.get_C(p);
        high = m_bwt_s.get_C(p + 1) - 1;

        // Deleting in S first
        if (low == high)
        {
            uint64_t o_remove_index = m_bwt_o.get_C(s) + m_bwt_s.ranky(low, s);
            uint64_t p_remove_index = m_bwt_p.get_C(o) + m_bwt_o.ranky(o_remove_index, o);

            m_bwt_s.remove_WT(low);
            m_bwt_o.remove_WT(o_remove_index);
            m_bwt_p.remove_WT(p_remove_index);

            // Update the bitvectors
            m_bwt_o.remove_C(m_bwt_o.select_C(s + 1) - 1);
            m_bwt_s.remove_C(m_bwt_s.select_C(p + 1) - 1);
            m_bwt_p.remove_C(m_bwt_p.select_C(o + 1) - 1);

            return ;
        }

        low = m_bwt_o.get_C(s) - 1 + m_bwt_s.ranky(low, s) + 1;
        high = m_bwt_o.get_C(s) - 1 + m_bwt_s.ranky(high + 1, s);


        // Deleting in O first
        if (low == high)
        {
            uint64_t p_remove_index = m_bwt_p.get_C(o) + m_bwt_o.ranky(low, o);
            uint64_t s_remove_index = m_bwt_s.get_C(p) + m_bwt_p.ranky(p_remove_index, p);

            m_bwt_o.remove_WT(low);
            m_bwt_p.remove_WT(p_remove_index);
            m_bwt_s.remove_WT(s_remove_index);

            // Update the bitvectors
            m_bwt_o.remove_C(m_bwt_o.select_C(s + 1) - 1);
            m_bwt_s.remove_C(m_bwt_s.select_C(p + 1) - 1);
            m_bwt_p.remove_C(m_bwt_p.select_C(o + 1) - 1);

            return;
        }

        low = m_bwt_p.get_C(o) - 1 + m_bwt_o.ranky(low, o) + 1;
        high = m_bwt_p.get_C(o) - 1 + m_bwt_o.ranky(high + 1, o);


        // Deleting in P
        if (low <= high)
        {
            uint64_t s_remove_index = m_bwt_s.get_C(p) + m_bwt_p.ranky(low, p);
            uint64_t o_remove_index = m_bwt_o.get_C(s) + m_bwt_s.ranky(s_remove_index, s);

            m_bwt_p.remove_WT(low);
            m_bwt_s.remove_WT(s_remove_index);
            m_bwt_o.remove_WT(o_remove_index);

            // Update the bitvectors
            m_bwt_o.remove_C(m_bwt_o.select_C(s + 1) - 1);
            m_bwt_s.remove_C(m_bwt_s.select_C(p + 1) - 1);
            m_bwt_p.remove_C(m_bwt_p.select_C(o + 1) - 1);

            return;
        }

        m_n_triples--;
    }

    /**
     * @brief remove all the triples associated to node value x from the ring.
     *        Keeps the sorting and updates the bitvectors in the wavelet trees
     *
     *
     * @tparam
     * @param x The value of the node being removed
     *
     * @return the amount of triples deleted
     */
    template <class bwt_so_t, class bwt_p_t>
    uint64_t ring<bwt_so_t, bwt_p_t>::remove_node(uint64_t x)
    {
        tuple<uint64_t, uint64_t> restricted_range = {m_bwt_o.get_C(x), m_bwt_o.get_C(x + 1) - 1};
        uint64_t low = get<0>(restricted_range), high = get<1>(restricted_range);
        uint64_t ret_value = 0;
        uint64_t new_index = 0;
        uint64_t total_removed = high - low + 1;

        // Remove every triple with S=x
        for (uint64_t i = low; i <= high; i++)
        {
            // Remove from SPO
            ret_value = m_bwt_o.remove_node_and_return(low);
            new_index = m_bwt_p.get_C(ret_value) + m_bwt_o.ranky(low, ret_value);
            m_bwt_p.remove_C(m_bwt_p.select_C(ret_value + 1) - 1);

            // Remove from OSP
            ret_value = m_bwt_p.remove_node_and_return(new_index);
            new_index = m_bwt_s.get_C(ret_value) + m_bwt_p.ranky(new_index, ret_value);
            m_bwt_s.remove_C(m_bwt_s.select_C(ret_value + 1) - 1);

            // Remove from POS
            ret_value = m_bwt_s.remove_node_and_return(new_index);
            new_index = m_bwt_o.get_C(ret_value) + m_bwt_s.ranky(new_index, ret_value);
            m_bwt_o.remove_C(m_bwt_o.select_C(ret_value + 1) - 1);
        }

        low = m_bwt_p.get_C(x);
        high = m_bwt_p.get_C(x + 1) - 1;
        total_removed += high - low + 1;

        // Remove every triple with O=x
        for (uint64_t i = low; i <= high; i++)
        {
            // Remove from OSP
            ret_value = m_bwt_p.remove_node_and_return(low);
            new_index = m_bwt_s.get_C(ret_value) + m_bwt_p.ranky(low, ret_value);
            m_bwt_s.remove_C(m_bwt_s.select_C(ret_value + 1) - 1);

            // Remove from POS
            ret_value = m_bwt_s.remove_node_and_return(new_index);
            new_index = m_bwt_o.get_C(ret_value) + m_bwt_s.ranky(new_index, ret_value);
            m_bwt_o.remove_C(m_bwt_o.select_C(ret_value + 1) - 1);

            // Remove from SPO
            ret_value = m_bwt_o.remove_node_and_return(new_index);
            new_index = m_bwt_p.get_C(ret_value) + m_bwt_o.ranky(new_index, ret_value);
            m_bwt_p.remove_C(m_bwt_p.select_C(ret_value + 1) - 1);
        }
        m_n_triples -= total_removed;

        return total_removed;
    }

    /**
     * @brief remove all the triples associated to node value x from the ring.
     *        Keeps the sorting and updates the bitvectors in the wavelet trees.
     *        Checks for IDs that arent being used in
     *
     *
     * @tparam
     * @param x The value of the node being removed
     * @param so_removed Reference to a vector to store the SO IDs that arent being used anymore
     * @param p_removed Reference to a vector to store the predicate IDs that arent being used anymore
     *
     * @return the amount of triples deleted
     */
    template <class bwt_so_t, class bwt_p_t>
    uint64_t ring<bwt_so_t, bwt_p_t>::remove_node_with_check(uint64_t x, std::vector<uint64_t> &so_removed, std::vector<uint64_t> &p_removed)
    {
        tuple<uint64_t, uint64_t> restricted_range = {m_bwt_o.get_C(x), m_bwt_o.get_C(x + 1) - 1};
        uint64_t low = get<0>(restricted_range), high = get<1>(restricted_range);
        uint64_t ret_value = 0;
        uint64_t new_index = 0;
        uint64_t total_removed = 0;

        if (high >= low)
        {
            total_removed += high - low + 1;

            // Remove every triple with S=x
            for (uint64_t i = low; i <= high; i++)
            {
                // Remove from SPO
                ret_value = m_bwt_o.remove_node_and_return(low);
                new_index = m_bwt_p.get_C(ret_value) + m_bwt_o.ranky(low, ret_value);
                m_bwt_p.remove_C(m_bwt_p.select_C(ret_value + 1) - 1);

                if (m_bwt_o.nElems(ret_value) == 0 && m_bwt_p.nElems(ret_value) == 0)
                    so_removed.emplace_back(ret_value);

                // Remove from OSP
                ret_value = m_bwt_p.remove_node_and_return(new_index);
                new_index = m_bwt_s.get_C(ret_value) + m_bwt_p.ranky(new_index, ret_value);
                m_bwt_s.remove_C(m_bwt_s.select_C(ret_value + 1) - 1);

                if (m_bwt_s.nElems(ret_value) == 0)
                    p_removed.emplace_back(ret_value);

                // Remove from POS
                ret_value = m_bwt_s.remove_node_and_return(new_index);
                new_index = m_bwt_o.get_C(ret_value) + m_bwt_s.ranky(new_index, ret_value);
                m_bwt_o.remove_C(m_bwt_o.select_C(ret_value + 1) - 1);
            }
        }

        low = m_bwt_p.get_C(x);
        high = m_bwt_p.get_C(x + 1) - 1;

        if (high >= low)
        {
            total_removed += high - low + 1;

            // Remove every triple with O=x
            for (uint64_t i = low; i <= high; i++)
            {
                // Remove from OSP
                ret_value = m_bwt_p.remove_node_and_return(low);
                new_index = m_bwt_s.get_C(ret_value) + m_bwt_p.ranky(low, ret_value);
                m_bwt_s.remove_C(m_bwt_s.select_C(ret_value + 1) - 1);

                if (m_bwt_s.nElems(ret_value) == 0)
                    p_removed.emplace_back(ret_value);

                // Remove from POS
                ret_value = m_bwt_s.remove_node_and_return(new_index);
                new_index = m_bwt_o.get_C(ret_value) + m_bwt_s.ranky(new_index, ret_value);
                m_bwt_o.remove_C(m_bwt_o.select_C(ret_value + 1) - 1);

                if (m_bwt_p.nElems(ret_value) == 0 && m_bwt_o.nElems(ret_value) == 0)
                    so_removed.emplace_back(ret_value);

                // Remove from SPO
                ret_value = m_bwt_o.remove_node_and_return(new_index);
                new_index = m_bwt_p.get_C(ret_value) + m_bwt_o.ranky(new_index, ret_value);
                m_bwt_p.remove_C(m_bwt_p.select_C(ret_value + 1) - 1);
            }
        }
        m_n_triples -= total_removed;

        return total_removed;
    }

    template <class bwt_so_t, class bwt_p_t>
    uint64_t ring<bwt_so_t, bwt_p_t>::bit_size()
    {
        uint64_t bytes = 0;
        bytes += m_bwt_o.bit_size();
        bytes += m_bwt_s.bit_size();
        bytes += m_bwt_p.bit_size();
        bytes += sizeof(size_type) * 8 * 4;
        return bytes;
    }

    typedef ring<bwt_rrr, bwt_rrr> c_ring;
    typedef ring<bwt_plain, bwt_plain> ring_sel;     // with select
    typedef ring<bwt_dynamic, bwt_dynamic> ring_dyn; // dynamic
    typedef ring<big_bwt, big_bwt> medium_ring_dyn;  // dynamic
    typedef ring<bwt_dyn_amo, bwt_dyn_amo> ring_dyn_amo; // dynamic amortizado

}

#endif
