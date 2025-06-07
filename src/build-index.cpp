/*
 * build-index.cpp
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

#include <iostream>
#include "ring.hpp"
#include "dict_map.hpp"
#include "dict_map_avl.hpp"
#include <fstream>
#include <regex>
#include <sdsl/construct.hpp>
#include <ltj_algorithm.hpp>

using namespace std;

using namespace std::chrono;
using timer = std::chrono::high_resolution_clock;

template <class ring>
void build_index(const std::string &dataset, const std::string &output)
{
    vector<spo_triple> D, E;

    std::ifstream ifs(dataset);
    uint64_t s, p, o;
    do
    {
        ifs >> s >> p >> o;
        D.push_back(spo_triple(s, p, o));
    } while (!ifs.eof());

    D.shrink_to_fit();
    cout << "--Indexing " << D.size() << " triples" << endl;
    memory_monitor::start();
    auto start = timer::now();

    ring A(D);
    auto stop = timer::now();
    memory_monitor::stop();
    cout << "  Index built  " << sdsl::size_in_bytes(A) << " bytes" << endl;

    sdsl::store_to_file(A, output);
    cout << "Index saved" << endl;
    cout << duration_cast<seconds>(stop - start).count() << " seconds." << endl;
    cout << memory_monitor::peak() << " bytes." << endl;
}

template <class map>
void build_mapping(const std::string &dataset, std::vector<spo_triple> &D, const std::string &output)
{
    map so_mapping;
    map p_mapping;

    std::ifstream ifs(dataset);
    std::string line;
    std::vector<std::string> user_input(3);
    std::regex token_regex("(?:\".*\"|[^[:space:]])+");
    std::sregex_iterator reg_it;
    uint64_t i = 0;
    auto mapping_start = timer::now();
    size_t count = 0;
    while (ifs.good())
    {
        getline(ifs, line);
        if (line.size() == 0)
            break;
        for (reg_it = sregex_iterator(line.begin(), line.end(), token_regex), i = 0; i < 3; i++, reg_it++)
        {
            user_input[i] = (*reg_it).str();
        }
        D.push_back(spo_triple(
            so_mapping.get_or_insert(user_input[0]),
            p_mapping.get_or_insert(user_input[1]),
            so_mapping.get_or_insert(user_input[2])));
        
        if (++count%1000000 == 0) {
            cout << "tripleta " << count << ": " << user_input[0] << " " << user_input[1] << " " << user_input[2] << endl;
        }
    }
    auto mapping_stop = timer::now();
    cout << "  Mapping built" << endl;
    cout << "    SO mapping " << so_mapping.bit_size() / 8 << " bytes" << endl;
    cout << "    P mapping " << p_mapping.bit_size() / 8 << " bytes" << endl;
    cout << "  Mapping took " << duration_cast<seconds>(mapping_stop - mapping_start).count() << " seconds." << endl;

    osfstream so_out(output + ".so.mapping", std::ios::binary | std::ios::trunc | std::ios::out);
    so_mapping.serialize(so_out);
    cout << "SO Mapping saved" << endl;

    osfstream p_out(output + ".p.mapping", std::ios::binary | std::ios::trunc | std::ios::out);
    p_mapping.serialize(p_out);
    cout << "P Mapping saved" << endl;
}

template <class ring, class map>
void build_index_mapped(const std::string &dataset, const std::string &output)
{
    vector<spo_triple> D, E;

    build_mapping<map>(dataset, D, output);

    D.shrink_to_fit();

    cout << "--Indexing " << D.size() << " triples" << endl;
    memory_monitor::start();
    auto start = timer::now();
    ring A(D);
    auto stop = timer::now();
    memory_monitor::stop();
    cout << "  Index built  " << sdsl::size_in_bytes(A) << " bytes" << endl;
    sdsl::store_to_file(A, output);
    cout << "Index saved" << endl;
    cout << duration_cast<seconds>(stop - start).count() << " seconds." << endl;
    cout << memory_monitor::peak() << " bytes." << endl;
}

int main(int argc, char **argv)
{

    if (argc != 4)
    {
        std::cout << "Usage: " << argv[0] << " <dataset> <type> <output>" << std::endl;
        return 0;
    }

    std::string dataset = argv[1];
    std::string type = argv[2];
    std::string output = argv[3];

    if (type == "ring")
    {
        std::string index_name = output + "/ring.ring";
        build_index<ring::ring<>>(dataset, index_name);
    }
    else if (type == "c-ring")
    {
        std::string index_name = output + "/c-ring.ring";
        build_index<ring::c_ring>(dataset, index_name);
    }
    else if (type == "ring-sel")
    {
        std::string index_name = output + "/ring-sel.ring";
        build_index<ring::ring_sel>(dataset, index_name);
    }
    else if (type == "ring-dyn-basic")
    {
        std::string index_name = output + "/ring-dyn-basic.ring";
        build_index<ring::ring_dyn>(dataset, index_name);
    }
    else if (type == "ring-dyn")
    {
        std::string index_name = output + "/ring-dyn.ring";
        build_index<ring::medium_ring_dyn>(dataset, index_name);
    }
    else if (type == "ring-dyn-amo")
    {
        std::string index_name = output + "/ring-dyn-amo.ring";
        build_index<ring::ring_dyn_amo>(dataset, index_name);
    }
    else if (type == "ring-map") {
        std::string index_name = output + "/ring-map.ring";
        build_index_mapped<ring::ring<>, ring::basic_map>(dataset, index_name);
    }
    else if (type == "ring-dyn-map")
    {
        std::string index_name = output + "/ring-dyn-map.ring";
        build_index_mapped<ring::medium_ring_dyn, ring::basic_map>(dataset, index_name);
    }
    else if (type == "ring-dyn-amo-map") {
        std::string index_name = output + "/ring-dyn-amo-map.ring";
        build_index_mapped<ring::ring_dyn_amo, ring::basic_map>(dataset, index_name);
    }
    else if (type == "ring-map-avl") {
        std::string index_name = output + "/ring-map-avl.ring";
        build_index_mapped<ring::ring<>, ring::basic_map_avl>(dataset, index_name);
    }
    else if (type == "ring-dyn-map-avl") {
        std::string index_name = output + "/ring-dyn-map-avl.ring";
        build_index_mapped<ring::medium_ring_dyn, ring::basic_map_avl>(dataset, index_name);
    }
    else if (type == "ring-dyn-amo-map-avl") {
        std::string index_name = output + "/ring-dyn-amo-map-avl.ring";
        build_index_mapped<ring::ring_dyn_amo, ring::basic_map_avl>(dataset, index_name);
    }
    else
    {
        std::cout << "Usage: " << argv[0] << " <dataset> <type> <output>" << std::endl;
    }

    return 0;
}
