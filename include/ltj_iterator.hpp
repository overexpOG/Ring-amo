/*
 * ltj_iterator.hpp
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

#ifndef RING_LTJ_ITERATOR_HPP
#define RING_LTJ_ITERATOR_HPP

#define VERBOSE 0

namespace ring {

    template<class ring_t, class var_t, class cons_t>
    class ltj_iterator {

    public:
        typedef cons_t value_type;
        typedef var_t var_type;
        typedef ring_t ring_type;
        typedef uint64_t size_type;
        //enum state_type {s, p, o};
        //std::vector<value_type> leap_result_type;

    private:
        const triple_pattern *m_ptr_triple_pattern;
        ring_type *m_ptr_ring; //TODO: should be const
        bwt_interval m_i_s;
        bwt_interval m_i_p;
        bwt_interval m_i_o;
        value_type m_cur_s;
        value_type m_cur_p;
        value_type m_cur_o;
        bool m_is_empty = false;
        //TODO: ao mellor hai que meter o nivel para saber cando parar de facer down
        //std::stack<state_type> m_states;


        void copy(const ltj_iterator &o) {
            m_ptr_triple_pattern = o.m_ptr_triple_pattern;
            m_ptr_ring = o.m_ptr_ring;
            m_i_s = o.m_i_s;
            m_i_p = o.m_i_p;
            m_i_o = o.m_i_o;
            m_cur_s = o.m_cur_s;
            m_cur_p = o.m_cur_p;
            m_cur_o = o.m_cur_o;
            m_is_empty = o.m_is_empty;
        }

        inline bool is_variable_subject(var_type var) {
            return m_ptr_triple_pattern->term_s.is_variable && var == m_ptr_triple_pattern->term_s.value;
        }

        inline bool is_variable_predicate(var_type var) {
            return m_ptr_triple_pattern->term_p.is_variable && var == m_ptr_triple_pattern->term_p.value;
        }

        inline bool is_variable_object(var_type var) {
            return m_ptr_triple_pattern->term_o.is_variable && var == m_ptr_triple_pattern->term_o.value;
        }

    public:
        const bool &is_empty = m_is_empty;
        const bwt_interval &i_s = m_i_s;
        const bwt_interval &i_p = m_i_p;
        const bwt_interval &i_o = m_i_o;
        const value_type &cur_s = m_cur_s;
        const value_type &cur_p = m_cur_p;
        const value_type &cur_o = m_cur_o;

        ltj_iterator() = default;

        ltj_iterator(const triple_pattern *triple, ring_type *ring) {
            m_ptr_triple_pattern = triple;
            m_ptr_ring = ring;
            m_cur_s = -1;
            m_cur_p = -1;
            m_cur_o = -1;
            m_i_p = m_ptr_ring->open_POS();
            m_i_s = m_ptr_ring->open_SPO();
            m_i_o = m_ptr_ring->open_OSP();
            //Init current values and intervals according to the triple
            if (!m_ptr_triple_pattern->s_is_variable() && !m_ptr_triple_pattern->p_is_variable()
                && !m_ptr_triple_pattern->o_is_variable()) {
                //S->O->P to avoid forward steps

                //Interval in S
                auto s_aux = m_ptr_ring->next_S(m_i_s, m_ptr_triple_pattern->term_s.value);
                //Is the constant of S in m_i_s?
                if (s_aux != m_ptr_triple_pattern->term_s.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_s = s_aux;

                //Interval in O
                m_i_o = m_ptr_ring->down_S(s_aux);
                auto o_aux = m_ptr_ring->next_O_in_S(m_i_o, m_ptr_triple_pattern->term_o.value);
                //Is the constant of O in m_i_o?
                if (o_aux != m_ptr_triple_pattern->term_o.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_o = o_aux;

                //Interval in P
                m_i_p = m_ptr_ring->down_S_O(m_i_o, o_aux);
                auto p_aux = m_ptr_ring->next_P_in_SO(m_i_p, m_ptr_triple_pattern->term_p.value);
                //Is the constant of P in m_i_p?
                if (p_aux != m_ptr_triple_pattern->term_p.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_p = p_aux;

            } else if (!m_ptr_triple_pattern->s_is_variable() && !m_ptr_triple_pattern->p_is_variable()) {
                //P->S to avoid forward steps

                //Interval in P
                auto p_aux = m_ptr_ring->next_P(m_i_p, m_ptr_triple_pattern->term_p.value);
                //Is the constant of S in m_i_s?
                if (p_aux != m_ptr_triple_pattern->term_p.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_p = p_aux;

                //Interval in S
                m_i_s = m_ptr_ring->down_P(p_aux);
                auto s_aux = m_ptr_ring->next_S_in_P(m_i_s, m_ptr_triple_pattern->term_s.value);
                //Is the constant of O in m_i_o?
                if (s_aux != m_ptr_triple_pattern->term_s.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_s = s_aux;

                //Interval in O
                m_i_o = m_ptr_ring->down_P_S(m_i_s, s_aux);

            } else if (!m_ptr_triple_pattern->p_is_variable() && !m_ptr_triple_pattern->o_is_variable()) {
                //O->P to avoid forward steps

                //Interval in O
                auto o_aux = m_ptr_ring->next_O(m_i_o, m_ptr_triple_pattern->term_o.value);
                //Is the constant of S in m_i_s?
                if (o_aux != m_ptr_triple_pattern->term_o.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_o = o_aux;

                //Interval in P
                m_i_p = m_ptr_ring->down_O(o_aux);
                auto p_aux = m_ptr_ring->next_P_in_O(m_i_p, m_ptr_triple_pattern->term_p.value);
                //Is the constant of P in m_i_p?
                if (p_aux != m_ptr_triple_pattern->term_p.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_p = p_aux;

                //Interval in S
                m_i_s = m_ptr_ring->down_O_P(m_i_p, p_aux);

            } else if (!m_ptr_triple_pattern->s_is_variable() && !m_ptr_triple_pattern->o_is_variable()) {
                //S->O to avoid forward steps

                //Interval in S
                auto s_aux = m_ptr_ring->next_S(m_i_s, m_ptr_triple_pattern->term_s.value);
                //Is the constant of S in m_i_s?
                if (s_aux != m_ptr_triple_pattern->term_s.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_s = s_aux;

                //Interval in O
                m_i_o = m_ptr_ring->down_S(s_aux);
                auto o_aux = m_ptr_ring->next_O_in_S(m_i_o, m_ptr_triple_pattern->term_o.value);
                //Is the constant of O in m_i_o?
                if (o_aux != m_ptr_triple_pattern->term_o.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_o = o_aux;

                //Interval in O
                m_i_p = m_ptr_ring->down_S_O(m_i_o, o_aux);

            } else if (!m_ptr_triple_pattern->s_is_variable()) {

                //Interval in S
                auto s_aux = m_ptr_ring->next_S(m_i_s, m_ptr_triple_pattern->term_s.value);
                //Is the constant of S in m_i_s?
                if (s_aux != m_ptr_triple_pattern->term_s.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_s = s_aux;

                m_i_p = m_i_o = m_ptr_ring->down_S(s_aux);

            } else if (!m_ptr_triple_pattern->p_is_variable()) {

                //Interval in P
                auto p_aux = m_ptr_ring->next_P(m_i_p, m_ptr_triple_pattern->term_p.value);
                //Is the constant of P in m_i_p?
                if (p_aux != m_ptr_triple_pattern->term_p.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_p = p_aux;

                m_i_s = m_i_o = m_ptr_ring->down_P(p_aux);

            } else if (!m_ptr_triple_pattern->o_is_variable()) {

                //Interval in O
                auto o_aux = m_ptr_ring->next_O(m_i_o, m_ptr_triple_pattern->term_o.value);
                //Is the constant of P in m_i_p?
                if (o_aux != m_ptr_triple_pattern->term_o.value) {
                    m_is_empty = true;
                    return;
                }
                m_cur_o = o_aux;

                m_i_s = m_i_p = m_ptr_ring->down_O(o_aux);

            }
        }

        //! Copy constructor
        ltj_iterator(const ltj_iterator &o) {
            copy(o);
        }

        //! Move constructor
        ltj_iterator(ltj_iterator &&o) {
            *this = std::move(o);
        }

        //! Copy Operator=
        ltj_iterator &operator=(const ltj_iterator &o) {
            if (this != &o) {
                copy(o);
            }
            return *this;
        }

        //! Move Operator=
        ltj_iterator &operator=(ltj_iterator &&o) {
            if (this != &o) {
                m_ptr_triple_pattern = std::move(o.m_ptr_triple_pattern);
                m_ptr_ring = std::move(o.m_ptr_ring);
                m_i_s = std::move(o.m_i_s);
                m_i_p = std::move(o.m_i_p);
                m_i_o = std::move(o.m_i_o);
                m_cur_s = o.m_cur_s;
                m_cur_p = o.m_cur_p;
                m_cur_o = o.m_cur_o;
                m_is_empty = o.m_is_empty;
            }
            return *this;
        }

        void swap(ltj_iterator &o) {
            // m_bp.swap(bp_support.m_bp); use set_vector to set the supported bit_vector
            std::swap(m_ptr_triple_pattern, o.m_ptr_triple_pattern);
            std::swap(m_ptr_ring, o.m_ptr_ring);
            m_i_s.swap(o.m_i_s);
            m_i_o.swap(o.m_i_o);
            m_i_p.swap(o.m_i_p);
            std::swap(m_cur_s, o.m_cur_s);
            std::swap(m_cur_p, o.m_cur_p);
            std::swap(m_cur_o, o.m_cur_o);
            std::swap(m_is_empty, o.m_is_empty);
        }

        void down(var_type var, size_type c) { //Go down in the trie
            if (is_variable_subject(var)) {
                if (m_cur_o != -1 && m_cur_p != -1){
#if VERBOSE
                    std::cout << "Nothing to do" << std::endl;
#endif
                    return;
                }
                if (m_cur_o != -1) {
                    //OS->P
#if VERBOSE
                    std::cout << "down_O_S" << std::endl;
#endif
                    m_i_p = m_ptr_ring->down_O_S(m_i_s, m_cur_o, c);
                } else if (m_cur_p != -1) {
                    //PS->O
#if VERBOSE
                    std::cout << "down_P_S" << std::endl;
#endif
                    m_i_o = m_ptr_ring->down_P_S(m_i_s, c);
                } else {
                    //S->{OP,PO} same range in SOP and SPO
#if VERBOSE
                    std::cout << "down_S" << std::endl;
#endif
                    m_i_o = m_i_p = m_ptr_ring->down_S(c);
                }
                //m_states.emplace(state_type::s);
                m_cur_s = c;
            } else if (is_variable_predicate(var)) {
                if (m_cur_s != -1 && m_cur_o != -1){
#if VERBOSE
                    std::cout << "Nothing to do" << std::endl;
#endif
                    return;
                }
                if (m_cur_o != -1) {
                    //OP->S
#if VERBOSE
                    std::cout << "down_O_P" << std::endl;
#endif
                    m_i_s = m_ptr_ring->down_O_P(m_i_p, c);
                } else if (m_cur_s != -1) {
                    //SP->O
#if VERBOSE
                    std::cout << "down_S_P" << std::endl;
#endif
                    m_i_o = m_ptr_ring->down_S_P(m_i_p, m_cur_s, c);
                } else {
                    //P->{OS,SO} same range in POS and PSO
#if VERBOSE
                    std::cout << "down_P" << std::endl;
#endif
                    m_i_o = m_i_s = m_ptr_ring->down_P(c);
                }
                //m_states.emplace(state_type::p);
                m_cur_p = c;
            } else if (is_variable_object(var)) {
                if (m_cur_s != -1 && m_cur_p != -1){
#if VERBOSE
                    std::cout << "Nothing to do" << std::endl;
#endif
                    return;
                }
                if (m_cur_p != -1) {
                    //PO->S
#if VERBOSE
                    std::cout << "down_P_O" << std::endl;
#endif
                    m_i_s = m_ptr_ring->down_P_O(m_i_o, m_cur_p, c);
                } else if (m_cur_s != -1) {
                    //SO->P
#if VERBOSE
                    std::cout << "down_S_O" << std::endl;
#endif
                    m_i_p = m_ptr_ring->down_S_O(m_i_o, c);
                } else {
                    //O->{PS,SP} same range in OPS and OSP
#if VERBOSE
                    std::cout << "down_O" << std::endl;
#endif
                    m_i_p = m_i_s = m_ptr_ring->down_O(c);
                }
                //m_states.emplace(state_type::o);
                m_cur_o = c;
            }

        };


        void up(var_type var) { //Go up in the trie
            if (is_variable_subject(var)) {
                m_cur_s = -1;
#if VERBOSE
                std::cout << "Up in S" << std::endl;
#endif
            } else if (is_variable_predicate(var)) {
                m_cur_p = -1;
#if VERBOSE
                std::cout << "Up in P" << std::endl;
#endif
            } else if (is_variable_object(var)) {
                m_cur_o = -1;
#if VERBOSE
                std::cout << "Up in O" << std::endl;
#endif
            }

        };

        value_type leap(var_type var) { //Return the minimum in the range
            //0. Which term of our triple pattern is var
            if (is_variable_subject(var)) {
                //1. We have to go down through s
                if (m_cur_p != -1 && m_cur_o != -1) {
                    //PO->S
#if VERBOSE
                    std::cout << "min_S_in_PO" << std::endl;
#endif
                    return m_ptr_ring->min_S_in_PO(m_i_s);
                } else if (m_cur_o != -1) {
                    //O->S
#if VERBOSE
                    std::cout << "min_S_in_O" << std::endl;
#endif
                    return m_ptr_ring->min_S_in_O(m_i_s, m_cur_o);
                } else if (m_cur_p != -1) {
                    //P->S
#if VERBOSE
                    std::cout << "min_S_in_P" << std::endl;
#endif
                    return m_ptr_ring->min_S_in_P(m_i_s);
                } else {
                    //S
#if VERBOSE
                    std::cout << "min_S" << std::endl;
#endif
                    return m_ptr_ring->min_S(m_i_s);
                }
            } else if (is_variable_predicate(var)) {
                //1. We have to go down in the trie of p
                if (m_cur_s != -1 && m_cur_o != -1) {
                    //SO->P
#if VERBOSE
                    std::cout << "min_P_in_SO" << std::endl;
#endif
                    return m_ptr_ring->min_P_in_SO(m_i_p);
                } else if (m_cur_s != -1) {
                    //S->P
#if VERBOSE
                    std::cout << "min_P_in_S" << std::endl;
#endif
                    return m_ptr_ring->min_P_in_S(m_i_p, m_cur_s);
                } else if (m_cur_o != -1) {
                    //O->P
#if VERBOSE
                    std::cout << "min_P_in_O" << std::endl;
#endif
                    return m_ptr_ring->min_P_in_O(m_i_p);
                } else {
                    //P
#if VERBOSE
                    std::cout << "min_P" << std::endl;
#endif
                    return m_ptr_ring->min_P(m_i_p);
                }
            } else if (is_variable_object(var)) {
                //1. We have to go down in the trie of o
                if (m_cur_s != -1 && m_cur_p != -1) {
                    //SP->O
#if VERBOSE
                    std::cout << "min_O_in_SP" << std::endl;
#endif
                    return m_ptr_ring->min_O_in_SP(m_i_o);
                } else if (m_cur_s != -1) {
                    //S->O
#if VERBOSE
                    std::cout << "min_O_in_S" << std::endl;
#endif
                    return m_ptr_ring->min_O_in_S(m_i_o);
                } else if (m_cur_p != -1) {
                    //P->O
#if VERBOSE
                    std::cout << "min_O_in_P" << std::endl;
#endif
                    return m_ptr_ring->min_O_in_P(m_i_o, m_cur_p);
                } else {
                    //O
#if VERBOSE
                    std::cout << "min_O" << std::endl;
#endif
                    return m_ptr_ring->min_O(m_i_o);
                }
            }
            return 0;
        };

        value_type leap(var_type var, size_type c) { //Return the next value greater or equal than c in the range
            //0. Which term of our triple pattern is var
            if (is_variable_subject(var)) {
                //1. We have to go down through s
                if (m_cur_p != -1 && m_cur_o != -1) {
                    //PO->S
#if VERBOSE
                    std::cout << "next_S_in_PO" << std::endl;
#endif
                    return m_ptr_ring->next_S_in_PO(m_i_s, c);
                } else if (m_cur_o != -1) {
                    //O->S
#if VERBOSE
                    std::cout << "next_S_in_O" << std::endl;
#endif
                    return m_ptr_ring->next_S_in_O(m_i_s, m_cur_o, c);
                } else if (m_cur_p != -1) {
                    //P->S
#if VERBOSE
                    std::cout << "next_S_in_P" << std::endl;
#endif
                    return m_ptr_ring->next_S_in_P(m_i_s, c);
                } else {
                    //S
#if VERBOSE
                    std::cout << "next_S" << std::endl;
#endif
                    return m_ptr_ring->next_S(m_i_s, c);
                }
            } else if (is_variable_predicate(var)) {
                //1. We have to go down in the trie of p
                if (m_cur_s != -1 && m_cur_o != -1) {
                    //SO->P
#if VERBOSE
                    std::cout << "next_P_in_SO" << std::endl;
#endif
                    return m_ptr_ring->next_P_in_SO(m_i_p, c);
                } else if (m_cur_s != -1) {
                    //S->P
#if VERBOSE
                    std::cout << "next_P_in_S" << std::endl;
#endif
                    return m_ptr_ring->next_P_in_S(m_i_p, m_cur_s, c);
                } else if (m_cur_o != -1) {
                    //O->P
#if VERBOSE
                    std::cout << "next_P_in_O" << std::endl;
#endif
                    return m_ptr_ring->next_P_in_O(m_i_p, c);
                } else {
                    //P
#if VERBOSE
                    std::cout << "next_P" << std::endl;
#endif
                    return m_ptr_ring->next_P(m_i_p, c);
                }
            } else if (is_variable_object(var)) {
                //1. We have to go down in the trie of o
                if (m_cur_s != -1 && m_cur_p != -1) {
                    //SP->O
#if VERBOSE
                    std::cout << "next_O_in_SP" << std::endl;
#endif
                    return m_ptr_ring->next_O_in_SP(m_i_o, c);
                } else if (m_cur_s != -1) {
                    //S->O
#if VERBOSE
                    std::cout << "next_O_in_S" << std::endl;
#endif
                    return m_ptr_ring->next_O_in_S(m_i_o, c);
                } else if (m_cur_p != -1) {
                    //P->O
#if VERBOSE
                    std::cout << "next_O_in_P" << std::endl;
#endif
                    return m_ptr_ring->next_O_in_P(m_i_o, m_cur_p, c);
                } else {
                    //O
#if VERBOSE
                    std::cout << "next_O" << std::endl;
#endif
                    return m_ptr_ring->next_O(m_i_o, c);
                }
            }
            return 0;
        }

        bool in_last_level(){
            return (m_cur_o !=-1 && m_cur_p != -1) || (m_cur_s !=-1 && m_cur_p != -1)
                    || (m_cur_o !=-1 && m_cur_s != -1);
        }

        //Solo funciona en último nivel, en otro caso habría que reajustar
        std::vector<uint64_t> seek_all(var_type var){
            if (is_variable_subject(var)){
                return m_ptr_ring->all_S_in_range(m_i_s);
            }else if (is_variable_predicate(var)){
                return m_ptr_ring->all_P_in_range(m_i_p);
            }else if (is_variable_object(var)){
                return m_ptr_ring->all_O_in_range(m_i_o);
            }
            return std::vector<uint64_t>();
        }
    };

}

#endif //RING_LTJ_ITERATOR_HPP
