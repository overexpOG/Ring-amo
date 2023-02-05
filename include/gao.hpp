/*
 * gao.hpp
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


#ifndef RING_GAO_HPP
#define RING_GAO_HPP

#include <ring.hpp>
#include <unordered_map>
#include <vector>
#include <utils.hpp>
#include <unordered_set>

namespace ring {

    namespace gao {

        template<class ring_t = ring<>, class var_t = uint8_t, class cons_t = uint64_t >
        class gao_size {

        public:
            typedef var_t var_type;
            typedef cons_t cons_type;
            typedef uint64_t size_type;
            typedef ring_t ring_type;
            typedef struct {
                var_type name;
                size_type weight;
                size_type n_triples;
                std::unordered_set<var_type> related;
            } info_var_type;

            typedef ltj_iterator<ring_type, var_type, cons_type> ltj_iter_type;
            typedef std::pair<size_type, var_type> pair_type;
            typedef std::priority_queue<pair_type, std::vector<pair_type>, greater<pair_type>> min_heap_type;

        private:
            const std::vector<triple_pattern>* m_ptr_triple_patterns;
            const std::vector<ltj_iter_type>* m_ptr_iterators;
            ring_type* m_ptr_ring;


            void var_to_vector(const var_type var, const size_type size,
                               std::unordered_map<var_type, size_type> &hash_table,
                               std::vector<info_var_type> &vec){

                auto it = hash_table.find(var);
                if(it == hash_table.end()){
                    info_var_type info;
                    info.name = var;
                    info.weight = size;
                    info.n_triples = 1;
                    vec.emplace_back(info);
                    hash_table.insert({var, vec.size()-1});
                }else{
                    info_var_type& info = vec[it->second];
                    ++info.n_triples;
                    if(info.weight > size){
                        info.weight = size;
                    }
                }
            }

            void var_to_related(const var_type var, const var_type rel,
                                std::unordered_map<var_type, size_type> &hash_table,
                                std::vector<info_var_type> &vec){

                auto pos_var = hash_table[var];
                vec[pos_var].related.insert(rel);
                auto pos_rel = hash_table[rel];
                vec[pos_rel].related.insert(var);
            }

            void fill_heap(const var_type var,
                           std::unordered_map<var_type, size_type> &hash_table,
                           std::vector<info_var_type> &vec,
                           std::vector<bool> &checked,
                           min_heap_type &heap){

                auto pos_var = hash_table[var];
                for(const auto &e : vec[pos_var].related){
                    auto pos_rel = hash_table[e];
                    if(!checked[pos_rel] && vec[pos_rel].n_triples > 1){
                        heap.push({vec[pos_rel].weight, e});
                        checked[pos_rel] = true;
                    }
                }
            }

            struct compare_var_info
            {
                inline bool operator() (const info_var_type& linfo, const info_var_type& rinfo)
                {
                    if(linfo.n_triples>1 && rinfo.n_triples==1){
                        return true;
                    }
                    if(linfo.n_triples==1 && rinfo.n_triples>1){
                        return false;
                    }
                    return linfo.weight < rinfo.weight;
                }
            };

        public:


            gao_size(const std::vector<triple_pattern>* triple_patterns,
                        const std::vector<ltj_iter_type>* iterators,
                        ring_type* r,
                        std::vector<var_type> &gao){
                m_ptr_triple_patterns = triple_patterns;
                m_ptr_iterators = iterators;
                m_ptr_ring = r;


                //1. Filling var_info with data about each variable
                //std::cout << "Filling... " << std::flush;
                std::vector<info_var_type> var_info;
                std::unordered_map<var_type, size_type> hash_table_position;
                size_type i = 0;
                for (const triple_pattern& triple_pattern : *m_ptr_triple_patterns) {
                    size_type size = util::get_size_interval(m_ptr_iterators->at(i));
                    bool s = false, p = false, o = false;
                    var_type var_s, var_p, var_o;
                    if(triple_pattern.s_is_variable()){
                        s = true;
                        var_s = (var_type) triple_pattern.term_s.value;
                        var_to_vector(var_s, size,hash_table_position, var_info);
                    }
                    if(triple_pattern.p_is_variable()){
                        p = true;
                        var_p = (var_type) triple_pattern.term_p.value;
                        var_to_vector(var_p, size,hash_table_position, var_info);
                    }
                    if(triple_pattern.o_is_variable()){
                        o = true;
                        var_o = triple_pattern.term_o.value;
                        var_to_vector(var_o, size,hash_table_position, var_info);
                    }

                    if(s && p){
                        var_to_related(var_s, var_p, hash_table_position, var_info);
                    }
                    if(s && o){
                        var_to_related(var_s, var_o, hash_table_position, var_info);
                    }
                    if(p && o){
                        var_to_related(var_p, var_o, hash_table_position, var_info);
                    }
                    ++i;
                }
                //std::cout << "Done. " << std::endl;

                //2. Sorting variables according to their weights.
                //std::cout << "Sorting... " << std::flush;
                std::sort(var_info.begin(), var_info.end(), compare_var_info());
                size_type lonely_start = var_info.size();
                for(i = 0; i < var_info.size(); ++i){
                    hash_table_position[var_info[i].name] = i;
                    if(var_info[i].n_triples == 1 && i < lonely_start){
                        lonely_start = i;
                    }
                }
                //std::cout << "Done. " << std::endl;

                //3. Choosing the variables
                i = 0;
                //std::cout << "Choosing GAO ... " << std::flush;
                std::vector<bool> checked(var_info.size(), false);
                gao.reserve(var_info.size());
                while(i < lonely_start){ //Related variables
                    if(!checked[i]){
                        gao.push_back(var_info[i].name); //Adding var to gao
                        checked[i] = true;
                        min_heap_type heap; //Stores the related variables that are related with the chosen ones
                        auto var_name = var_info[i].name;
                        fill_heap(var_name, hash_table_position, var_info, checked,heap);
                        while(!heap.empty()){
                            var_name = heap.top().second;
                            heap.pop();
                            gao.push_back(var_name);
                            fill_heap(var_name, hash_table_position, var_info, checked, heap);
                        }
                    }
                    ++i;
                }
                while(i < var_info.size()){ //Lonely variables
                    gao.push_back(var_info[i].name); //Adding var to gao
                    ++i;
                }
                //std::cout << "Done. " << std::endl;
            }

        };

    }
}

#endif //RING_GAO_HPP
