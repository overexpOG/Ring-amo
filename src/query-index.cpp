/*
 * query-index.cpp
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

#include <regex> 
#include <iostream>
#include <utility>
#include "ring.hpp"
#include "dict_map.hpp"
#include "dict_map_avl.hpp"
#include <chrono>
#include <triple_pattern.hpp>
#include <ltj_algorithm.hpp>
#include "utils.hpp"

using namespace std;

// #include<chrono>
// #include<ctime>

using namespace std::chrono;

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

std::vector<std::string> tokenizer(const std::string &input, const char &delimiter)
{
    std::stringstream stream(input);
    std::string token;
    std::vector<std::string> res;
    while (getline(stream, token, delimiter))
    {
        res.emplace_back(trim(token));
    }
    return res;
}

std::vector<std::string> parse_select(const std::string &input)
{
    std::vector<std::string> res;
    size_t start = input.find_first_of("{"),
           end = input.find_last_of("}");
    std::string query = input.substr(start + 1, end - start - 1);
    size_t index = 0, tmp_index = 0;
    while (tmp_index < query.size())
    {
        tmp_index = query.find(" . ", index);
        res.emplace_back(query.substr(index, tmp_index - index));
        index = tmp_index + 2;
    }
    return res;
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

bool is_variable(string &s)
{
    return (s.at(0) == '?');
}

uint8_t get_variable(string &s, std::unordered_map<std::string, uint8_t> &hash_table_vars)
{
    auto var = s.substr(1);
    auto it = hash_table_vars.find(var);
    if (it == hash_table_vars.end())
    {
        uint8_t id = hash_table_vars.size();
        hash_table_vars.insert({var, id});
        return id;
    }
    else
    {
        return it->second;
    }
}

uint64_t get_constant(string &s)
{
    return std::stoull(s);
}

ring::triple_pattern get_triple(string &s, std::unordered_map<std::string, uint8_t> &hash_table_vars)
{
    vector<string> terms = tokenizer(s, ' ');

    ring::triple_pattern triple;
    if (is_variable(terms[0]))
    {
        triple.var_s(get_variable(terms[0], hash_table_vars));
    }
    else
    {
        triple.const_s(get_constant(terms[0]));
    }
    if (is_variable(terms[1]))
    {
        triple.var_p(get_variable(terms[1], hash_table_vars));
    }
    else
    {
        triple.const_p(get_constant(terms[1]));
    }
    if (is_variable(terms[2]))
    {
        triple.var_o(get_variable(terms[2], hash_table_vars));
    }
    else
    {
        triple.const_o(get_constant(terms[2]));
    }
    return triple;
}

template <class map_type>
ring::triple_pattern get_user_triple(string &s, std::unordered_map<std::string, uint8_t> &hash_table_vars, map_type &so_mapping, map_type &p_mapping)
{
    std::regex token_regex("(?:\".*\"|[^[:space:]])+");
    vector<string> terms = regex_tokenizer(s, token_regex);

    ring::triple_pattern triple;
    if (is_variable(terms[0]))
    {
        triple.var_s(get_variable(terms[0], hash_table_vars));
    }
    else
    {
        triple.const_s(so_mapping.locate(terms[0]).second);
    }
    if (is_variable(terms[1]))
    {
        triple.var_p(get_variable(terms[1], hash_table_vars));
    }
    else
    {
        triple.const_p(p_mapping.locate(terms[1]).second);
    }
    if (is_variable(terms[2]))
    {
        triple.var_o(get_variable(terms[2], hash_table_vars));
    }
    else
    {
        triple.const_o(so_mapping.locate(terms[2]).second);
    }
    return triple;
}

std::string get_type(const std::string &file)
{
    auto p = file.find_last_of('.');
    return file.substr(p + 1);
}

template <class ring_type>
void query(const std::string &file, const std::string &queries)
{
    vector<string> dummy_queries;
    bool result = get_file_content(queries, dummy_queries);

    ring_type graph;

    cout << " Loading the index...";
    fflush(stdout);
    sdsl::load_from_file(graph, file);

    cout << endl
         << " Index loaded " << sdsl::size_in_bytes(graph) << " bytes" << endl;

    std::ifstream ifs;
    uint64_t nQ = 0;

    high_resolution_clock::time_point start, stop;
    double total_time = 0.0;
    duration<double> time_span;

    if (result)
    {
        for (string &query_string : dummy_queries)
        {
            std::unordered_map<std::string, uint8_t> hash_table_vars;
            std::vector<ring::triple_pattern> query;
            vector<string> tokens_query = tokenizer(query_string, '.');
            for (string &token : tokens_query)
            {
                auto triple_pattern = get_triple(token, hash_table_vars);
                query.push_back(triple_pattern);
            }

            start = high_resolution_clock::now();

            ring::ltj_algorithm<ring_type> ltj(&query, &graph);
            typedef std::vector<typename ring::ltj_algorithm<>::tuple_type> results_type;
            results_type res;
            
            ltj.join(res, 1000, 600);
            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            total_time = time_span.count();

            cout << nQ << ";" << res.size() << ";" << (unsigned long long)(total_time * 1000000000ULL) << endl;
            nQ++;
        }
    }
}

template <class ring_type, class map_type>
void mapped_query(const std::string &file, const std::string &so_mapping_file, const std::string &p_mapping_file, const std::string &queries)
{
    vector<string> dummy_queries;

    bool result = get_file_content(queries, dummy_queries);

    // Load Dictionary Mapping
    map_type so_mapping;
    std::ifstream so_infs(so_mapping_file, std::ios::binary | std::ios::in);
    so_mapping.load(so_infs);

    cout << endl
         << " SO Mapping loaded " << so_mapping.bit_size() / 8 << " bytes" << endl;

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
    double total_time = 0.0, forward_trad = 0.0, backward_trad = 0.0;
    duration<double> time_span;

    if (result)
    {
        for (string &query_string : dummy_queries)
        {
            std::unordered_map<std::string, uint8_t> hash_table_vars;
            std::vector<ring::triple_pattern> query;
            vector<string> tokens_query = parse_select(query_string);

            start = high_resolution_clock::now();
            for (string &token : tokens_query)
            {
                auto triple_pattern = get_user_triple<map_type>(token, hash_table_vars, so_mapping, p_mapping);
                query.push_back(triple_pattern);
            }

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            forward_trad = time_span.count();

            start = high_resolution_clock::now();

            ring::ltj_algorithm<ring_type> ltj(&query, &graph);

            typedef std::vector<typename ring::ltj_algorithm<>::tuple_type> results_type;
            results_type res;
            ltj.join(res, 1000, 600);

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            total_time = time_span.count();

            start = high_resolution_clock::now();

            if (res.size() > 0)
            {
                for (auto r : res)
                {
                    for (auto x : r)
                    {
                        so_mapping.extract(get<1>(x));
                    }
                }
            }

            stop = high_resolution_clock::now();
            time_span = duration_cast<microseconds>(stop - start);
            backward_trad = time_span.count();

            cout << nQ << ";" << res.size() << ";" << (unsigned long long)(total_time * 1000000000ULL);
            cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
            cout << ";" << (unsigned long long)(backward_trad * 1000000000ULL) << endl;
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
        if (type == "ring")
        {
            query<ring::ring<>>(index, queries);
        }
        else if (type == "c-ring")
        {
            query<ring::c_ring>(index, queries);
        }
        else if (type == "ring-sel")
        {
            query<ring::ring_sel>(index, queries);
        }
        else if (type == "ring-dyn-basic")
        {
            query<ring::ring_dyn>(index, queries);
        }
        else if (type == "ring-dyn")
        {
            query<ring::medium_ring_dyn>(index, queries);
        }
        else if (type == "ring-dyn-amo")
        {
            query<ring::ring_dyn_amo>(index, queries);
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
        if (type == "ring")
        {
            mapped_query<ring::ring<>, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "c-ring")
        {
            mapped_query<ring::c_ring, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if ("ring-avl")
        {
            mapped_query<ring::ring<>, ring::basic_map_avl>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "ring-sel")
        {
            mapped_query<ring::ring_sel, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "ring-dyn-basic")
        {
            mapped_query<ring::ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "ring-dyn")
        {
            mapped_query<ring::medium_ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else if (type == "ring-dyn-amo")
        {
            mapped_query<ring::ring_dyn_amo, ring::basic_map>(index, so_mapping, p_mapping, queries);
        }
        else
        {
            std::cout << "Type of index: " << type << " is not supported." << std::endl;
        }
    }

    return 0;
}
