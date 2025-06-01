/*
 * delete-node.cpp
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
#include <utility>
#include "ring.hpp"
#include "dict_map.hpp"
#include <chrono>
#include <triple_pattern.hpp>
#include <ltj_algorithm.hpp>
#include "utils.hpp"

using namespace std;

// #include<chrono>
// #include<ctime>

using namespace std::chrono;

bool get_values_from_file(string filename, vector<uint64_t> &vector_of_values)
{
    // Open the File
    ifstream in(filename.c_str());
    // Check if object is valid
    if (!in)
    {
        cerr << "Cannot open the File : " << filename << endl;
        return false;
    }
    // Read the next line from File until it reaches the end.
    uint64_t v;
    do
    {
        in >> v;
        vector_of_values.push_back(v);
    } while (!in.eof());

    // Close The File
    in.close();
    return true;
}

bool get_file_content(string filename, vector<string> &vector_of_strings)
{
    // Open the File
    ifstream in(filename.c_str());
    // Check if object is valid
    if (!in)
    {
        cerr << "Cannot open the File : " << filename << endl;
        return false;
    }
    string str;
    // Read the next line from File until it reaches the end.
    while (getline(in, str))
    {
        // Line contains string of length > 0 then save it in vector
        if (str.size() > 0)
            vector_of_strings.push_back(str);
    }
    // Close The File
    in.close();
    return true;
}

string get_node_value(const std::string &input)
{
    size_t start = input.find("?s=") + 3;
    size_t end = input.find_first_of(")", start);
    return input.substr(start, end - start);
}

std::string get_type(const std::string &file)
{
    auto p = file.find_last_of('.');
    return file.substr(p + 1);
}

template <class ring_type>
void delete_query(const std::string &file, const std::string &queries)
{
    vector<uint64_t> dummy_queries;
    bool result = get_values_from_file(queries, dummy_queries);

    ring_type graph;

    cout << " Loading the index...";
    fflush(stdout);
    sdsl::load_from_file(graph, file);

    cout << endl
         << " Index loaded " << sdsl::size_in_bytes(graph) << " bytes" << endl;

    uint64_t nQ = 0;

    high_resolution_clock::time_point start, stop;
    double total_time = 0.0;
    duration<double> time_span;

    if (result)
    {
        for (uint64_t &query_value : dummy_queries)
        {
            start = high_resolution_clock::now();

            uint64_t total_removed = graph.remove_node(query_value);

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            total_time = time_span.count();

            cout << nQ << ";" << total_removed << ";" << (unsigned long long)(total_time * 1000000000ULL) << endl;
            nQ++;
        }
    }
}

template <class ring_type, class map_type>
void mapped_delete_query(const std::string &file, const std::string &so_mapping_file, const std::string &p_mapping_file, const std::string &queries)
{
    vector<string> dummy_queries;

    bool result = get_file_content(queries, dummy_queries);

    // Load SO Dictionary Mapping
    map_type so_mapping;
    std::ifstream so_infs(so_mapping_file, std::ios::binary | std::ios::in);
    so_mapping.load(so_infs);

    cout << endl
         << " SO Mapping loaded " << so_mapping.bit_size() / 8 << " bytes" << endl;

    // Load P Dictionary Mapping
    map_type p_mapping;
    std::ifstream p_infs(p_mapping_file, std::ios::binary | std::ios::in);
    p_mapping.load(p_infs);

    cout << endl
         << " P Mapping loaded " << p_mapping.bit_size() / 8 << " bytes" << endl;

    ring_type graph;

    cout << " Loading the index...";
    fflush(stdout);
    sdsl::load_from_file(graph, file);

    cout << endl
         << " Index loaded " << sdsl::size_in_bytes(graph) << " bytes" << endl;

    std::ifstream ifs;
    uint64_t nQ = 0;

    high_resolution_clock::time_point start, stop;
    double total_time = 0.0, forward_trad = 0.0, backward_time = 0.0;
    duration<double> time_span;

    if (result)
    {
        for (string &query_string : dummy_queries)
        {
            string node_value = get_node_value(query_string);

            start = high_resolution_clock::now();

            uint64_t node_id = so_mapping.eliminate(node_value);

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            forward_trad = time_span.count();

            vector<uint64_t> so_removed_ids;
            vector<uint64_t> p_removed_ids;

            start = high_resolution_clock::now();

            uint64_t total_removed = graph.remove_node_with_check(node_id, so_removed_ids, p_removed_ids);

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            total_time = time_span.count();

            start = high_resolution_clock::now();

            for (uint64_t id : so_removed_ids)
            {
                so_mapping.eliminate(id);
            }

            for (uint64_t id : p_removed_ids)
            {
                p_mapping.eliminate(id);
            }

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            backward_time = time_span.count();

            cout << nQ << ";" << total_removed << ";" << (unsigned long long)(total_time * 1000000000ULL);
            cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
            cout << ";" << (unsigned long long)(backward_time * 1000000000ULL) << endl;
            nQ++;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 5)
    {
        std::cout << "Usage: " << argv[0] << " <index> <queries>" << std::endl;
        return 0;
    }

    std::string index = argv[1];
    std::string queries = argv[2];
    std::string type = get_type(index);

    if (argc == 3)
    {
        if (type == "ring-dyn-basic")
        {
            delete_query<ring::ring_dyn>(index, queries);
        }
        else if (type == "ring-dyn")
        {
            delete_query<ring::medium_ring_dyn>(index, queries);
        }
        else if (type == "ring-dyn-amo")
        {
            delete_query<ring::ring_dyn_amo>(index, queries);
        }
        else
        {
            std::cout << "Type of index: " << type << " is not supported." << std::endl;
        }
    }

    if (argc == 5)
    {
        std::string so_mapping = argv[3];
        std::string p_mapping = argv[4];
        if (type == "ring-dyn-basic")
        {
            mapped_delete_query<ring::ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "ring-dyn")
        {
            mapped_delete_query<ring::medium_ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "ring-dyn-amo")
        {
            mapped_delete_query<ring::ring_dyn_amo, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else
        {
            std::cout << "Type of index: " << type << " is not supported." << std::endl;
        }
    }

    return 0;
}
