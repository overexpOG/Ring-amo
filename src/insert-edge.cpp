/*
 * insert-edge.cpp
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
#include <regex>

using namespace std;

// #include<chrono>
// #include<ctime>

using namespace std::chrono;

bool get_triples_from_file(string filename, vector<spo_triple> &vector_of_triples)
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
    uint64_t s, p, o;
    do
    {
        in >> s >> p >> o;
        vector_of_triples.push_back(spo_triple(s, p, o));
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

std::string ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(' ');
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(' ');
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string &s)
{
    return rtrim(ltrim(s));
}

std::vector<std::string> regex_tokenizer(const std::string &input, std::regex regex_token)
{
    std::smatch match;
    std::vector<std::string> res;

    for (
        std::sregex_iterator reg_it = std::sregex_iterator(input.begin(), input.end(), regex_token);
        reg_it != std::sregex_iterator();
        reg_it++)
    {
        match = *reg_it;
        res.emplace_back(trim(match.str()));
    }
    return res;
}

template <class map_type>
spo_triple parse_insert(const std::string &input, const map_type &so_mapping, const map_type &p_mapping)
{
    vector<string> res;
    size_t start = input.find_first_of("{"),
           end = input.find_last_of("}");
    string query = input.substr(start + 1, end - start - 1);
    regex token_regex("(?:\".*\"|[^[:space:]])+");
    vector<string> terms = regex_tokenizer(query, token_regex);
    return {so_mapping.get_or_insert(terms[0]), p_mapping.get_or_insert(terms[1]), so_mapping.get_or_insert(terms[2])};
}

std::string get_type(const std::string &file)
{
    auto p = file.find_last_of('.');
    return file.substr(p + 1);
}

template <class ring_type>
void insert_query(const std::string &file, const std::string &queries)
{
    vector<spo_triple> dummy_queries;
    bool result = get_triples_from_file(queries, dummy_queries);

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
        for (spo_triple &query_triple : dummy_queries)
        {
            start = high_resolution_clock::now();

            graph.insert(query_triple);

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            total_time = time_span.count();

            cout << nQ << ";" << (unsigned long long)(total_time * 1000000000ULL) << endl;
            nQ++;
        }
    }
}

template <class ring_type, class map_type>
void mapped_insert_query(const std::string &file, const std::string &so_mapping_file, const std::string &p_mapping_file, const std::string &queries)
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
    double total_time = 0.0, forward_trad = 0.0;
    duration<double> time_span;

    if (result)
    {
        for (string &query_string : dummy_queries)
        {
            start = high_resolution_clock::now();

            spo_triple query_triple = parse_insert<map_type>(query_string, so_mapping, p_mapping);

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            forward_trad = time_span.count();

            start = high_resolution_clock::now();

            graph.insert(query_triple);

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            total_time = time_span.count();

            cout << nQ << ";" << (unsigned long long)(total_time * 1000000000ULL);
            cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL) << endl;
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
            insert_query<ring::ring_dyn>(index, queries);
        }
        else if (type == "ring-dyn")
        {
            insert_query<ring::medium_ring_dyn>(index, queries);
        }
        else if (type == "ring-dyn-amo")
        {
            insert_query<ring::ring_dyn_amo>(index, queries);
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
            mapped_insert_query<ring::ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "ring-dyn")
        {
            mapped_insert_query<ring::medium_ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else
        {
            std::cout << "Type of index: " << type << " is not supported." << std::endl;
        }
    }

    return 0;
}
