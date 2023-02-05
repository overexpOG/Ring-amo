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
#include <fstream>
#include <sdsl/construct.hpp>
#include <ltj_algorithm.hpp>

using namespace std;

using namespace std::chrono;
using timer = std::chrono::high_resolution_clock;

template<class ring>
void build_index(const std::string &dataset, const std::string &output){
    vector<spo_triple> D, E;

    std::ifstream ifs(dataset);
    uint64_t s, p , o;
    do {
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
    cout << duration_cast<seconds>(stop-start).count() << " seconds." << endl;
    cout << memory_monitor::peak() << " bytes." << endl;
}

int main(int argc, char **argv)
{

    if(argc != 3){
        std::cout << "Usage: " << argv[0] << " <dataset> [ring|c-ring|ring-sel]" << std::endl;
        return 0;
    }

    std::string dataset = argv[1];
    std::string type    = argv[2];
    if(type == "ring"){
        std::string index_name = dataset + ".ring";
        build_index<ring::ring<>>(dataset, index_name);
    }else if (type == "c-ring"){
        std::string index_name = dataset + ".c-ring";
        build_index<ring::c_ring>(dataset, index_name);
    }else if (type == "ring-sel"){
        std::string index_name = dataset + ".ring-sel";
        build_index<ring::ring_sel>(dataset, index_name);
    }else if (type == "ring-dyn"){
        std::string index_name = dataset + ".ring-dyn";
        build_index<ring::ring_dyn>(dataset, index_name);
    }else if (type == "ring-dyn-m"){
        std::string index_name = dataset + ".ring-dyn-m";
        build_index<ring::medium_ring_dyn>(dataset, index_name);
    }else{
        std::cout << "Usage: " << argv[0] << " <dataset> [ring|c-ring|ring-sel]" << std::endl;
    }

    return 0;
}

