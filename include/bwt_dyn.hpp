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

#ifndef BWT_DYN_T
#define BWT_DYN_T

#include <dynamic/dynamic.hpp>
#include "configuration.hpp"

using namespace std;

typedef dyn::wm_string<dyn::succinct_bitvector<dyn::spsi<dyn::packed_bit_vector,2048,256>>> chosen_one_bwt;

namespace ring
{

  template <class bwt_bit_vector_t = dyn::suc_bv,
            class bwt_wm_type = dyn::wm_str>
  class bwt_dyn
  {

  public:
    typedef uint64_t value_type;
    typedef uint64_t size_type;
    typedef bwt_bit_vector_t c_type; // BITVECTORS
    typedef bwt_wm_type bwt_type;

  private:
    bwt_type m_L;
    c_type m_C;
    uint32_t m_sigma;

    void copy(const bwt_dyn &o)
    {
      m_L = o.m_L;
      m_C = o.m_C;
    }

  public:
    //! Dfault constructor
    bwt_dyn()
    {
      m_L = bwt_type();
      m_C = c_type();
    }

    bwt_dyn(const vector<uint64_t> &L, const vector<uint64_t> &C, uint64_t sigma)
    {
      m_sigma = sigma;
      // Building the wavelet matrix
      m_L = bwt_type(C.size() - 2, L);
      // Building C and its rank and select structures
      m_C = c_type();
      for (uint64_t i = 0; i < C[C.size() - 1] + 1 + C.size(); i++)
        m_C.insert0(0);
      for (uint64_t i = 0; i < C.size(); i++)
      {
        m_C.set(C[i] + i);
      }
    }


    bwt_dyn(const int_vector<> &L, const vector<uint64_t> &C, uint64_t sigma)
    {
      m_sigma = sigma;
      // Building the wavelet matrix
      vector<uint64_t> tmp(L.size());
      std::copy(L.begin(), L.end(), tmp.begin());
      m_L = bwt_type(sigma, tmp);
      // Building C and its rank and select structures
      m_C = c_type();
      uint64_t c_index = 0;
      for (uint64_t i = 0; i < C[C.size() - 1] + 1 + C.size(); i++) {
        if (c_index < C.size() && c_index + C[c_index] == i) {
          m_C.insert1(i);
          c_index++;
        } else {
          m_C.insert0(i);
        }
      }
    }

    //! Copy constructor
    bwt_dyn(const bwt_dyn &o)
    {
      copy(o);
    }

    //! Move constructor
    bwt_dyn(bwt_dyn &&o)
    {
      *this = move(o);
    }

    //! Copy Operator=
    bwt_dyn &operator=(const bwt_dyn &o)
    {
      if (this != &o)
      {
        copy(o);
      }
      return *this;
    }

    //! Move Operator=
    bwt_dyn &operator=(bwt_dyn &&o)
    {
      if (this != &o)
      {
        m_L = move(o.m_L);
        m_C = move(o.m_C);
      }
      return *this;
    }

    void swap(bwt_dyn &o)
    {
      swap(m_L, o.m_L);
      swap(m_C, o.m_C);
    }

    // //! Serializes the data structure into the given ostream
    size_type serialize(ostream &out, sdsl::structure_tree_node *v = nullptr, string name = "") const
    {
      sdsl::structure_tree_node *child = sdsl::structure_tree::add_child(v, name, "bwt_dyn");
      size_type written_bytes = 0;
      sdsl::structure_tree_node *l_child = sdsl::structure_tree::add_child(child, "L", "wm_int");
      size_type l_written_bytes = m_L.serialize(out);
      written_bytes += l_written_bytes;
      sdsl::structure_tree::add_size(l_child, l_written_bytes);
      sdsl::structure_tree_node *c_child = sdsl::structure_tree::add_child(child, "C", "dyn_bv");
      size_type c_written_bytes = m_C.serialize(out);
      written_bytes += c_written_bytes;
      sdsl::structure_tree::add_size(c_child, c_written_bytes);
      sdsl::structure_tree::add_size(child, written_bytes);
      return written_bytes;
    }

    void load(istream &in)
    {
      m_L.load(in);
      m_C.load(in);
    }

    uint64_t triple_amount()
    {
      return m_L.bit_arrays.at(0).size();
    }

    void statistics() {
      m_L.statistics();
    }

    uint64_t bit_size()
    {
      return m_L.bit_size() + m_C.bit_size();
    }

     uint64_t alphabet_size() {
      return m_L.sigma - 1;
    }

    void increment_alphabet() {
      m_L.increment_alphabet();
    }

    void print_tree()
    {
      for (uint64_t i = 0; i < m_L.bit_arrays.size(); i++)
      {
        for (uint64_t j = 0; j < m_L.bit_arrays.at(i).size(); j++)
        {
          cout << m_L.bit_arrays.at(i)[j] << " ";
        }
        cout << endl;
      }
    }

    void print_C()
    {
      cout << "C bitvector" << endl;
      for (uint64_t i = 0; i < m_C.size(); i++) {
          cout << m_C[i] << " ";
      }
      cout << endl;
    }

    // Operations
    //  Get the value of v in C
    inline size_type get_C(const uint64_t v) const
    {
      return m_C.select1(v) - v;
    }


    inline uint64_t LF(uint64_t i)
    {
      uint64_t s = m_L[i];
      return get_C(s) + m_L.rank(i, s) - 1;
    }

    //! Amount of repeated instances of val in the WT
    uint64_t nElems(uint64_t val)
    {
      return get_C(val + 1) - get_C(val);
    }

    pair<uint64_t, uint64_t>
    backward_step(uint64_t left_end, uint64_t right_end, uint64_t value)
    {
      return {m_L.rank(left_end, value), m_L.rank(right_end + 1, value) - 1};
    }

    inline uint64_t bsearch_C(uint64_t value)
    {
      return m_C.rank(m_C.select0(value));
    }

    // Creo que no se usa
    inline uint64_t ranky(uint64_t pos, uint64_t val)
    {
      return m_L.rank(pos, val);
    }

    inline uint64_t rank(uint64_t pos, uint64_t val)
    {
      return m_L.rank(get_C(pos), val);
    }

    // Creo que no se usa
    inline uint64_t select(uint64_t _rank, uint64_t val)
    {
      return m_L.select(_rank, val) - 1;
    }

    inline pair<uint64_t, uint64_t> select_next(uint64_t pos, uint64_t val, uint64_t n_elems)
    {
      return m_L.select_next(get_C(pos), val, n_elems);
    }

    inline uint64_t min_in_range(uint64_t l, uint64_t r)
    {
      return m_L.range_minimum_query(l, r);
    }

    inline uint64_t range_next_value(uint64_t x, uint64_t l, uint64_t r)
    {
      return m_L.range_next_value(x, l, r);
    }

    vector<uint64_t>
    values_in_range(uint64_t pos_min, uint64_t pos_max)
    {
      return m_L.all_values_in_range(pos_min, pos_max);
    }

    // backward search for pattern of length 1
    pair<uint64_t, uint64_t> backward_search_1_interval(uint64_t P) const
    {
      return {get_C(P), get_C(P + 1) - 1};
    }

    // backward search for pattern of length 1
    pair<uint64_t, uint64_t> backward_search_1_rank(uint64_t P, uint64_t S) const
    {
      return {m_L.rank(get_C(P), S), m_L.rank(get_C(P + 1), S)};
    }

    // backward search for pattern PQ of length 2
    // returns an empty interval if search is unsuccessful
    pair<uint64_t, uint64_t>
    backward_search_2_interval(uint64_t P, pair<uint64_t, uint64_t> &I) const
    {
      return {get_C(P) + I.first, get_C(P) + I.second - 1};
    }

    pair<uint64_t, uint64_t>
    backward_search_2_rank(uint64_t P, uint64_t S, pair<uint64_t, uint64_t> &I) const
    {
      uint64_t c = get_C(P);
      return {m_L.rank(c + I.first, S), m_L.rank(c + I.second, S)};
    }

    inline pair<uint64_t, uint64_t> inverse_select(uint64_t pos)
    {
      return m_L.inverse_select(pos);
    }

    inline uint64_t operator[](uint64_t i)
    {
      return m_L[i];
    }

    uint64_t select_C(uint64_t s) {
      return m_C.select1(s);
    }

    void insert_C(uint64_t s, bool b) {
      m_C.insert(s, b);
    }

    void remove_C(uint64_t i) {
      m_C.remove(i);
    }

    void push_back_C(bool b) {
      m_C.insert(m_C.size() - 1, b);
    }

    void insert_WT(uint64_t i, uint64_t v) {
      m_L.insert(i, v);
    }

    void remove_WT(uint64_t i) {
      m_L.remove(i);
    }

    uint64_t remove_node_and_return(uint64_t i) {
      return m_L.remove_and_return(i);
    }
  };

  typedef bwt_dyn<> bwt_dynamic;
  typedef bwt_dyn<dyn::suc_bv, chosen_one_bwt> big_bwt;
}

#endif
