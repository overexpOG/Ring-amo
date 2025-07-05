#include <regex>
#include <unordered_set>
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

enum class QueryType {
    SELECT,
    INSERT,
    DELETE_EDGE,
    DELETE_NODE
};

std::unordered_set<std::string> select_type = {"J3", "J4", "P2", "P3", "P4", "S1", "S2", "S3", "S4", "T2", "T3", "T4", "TI2", "TI3", "TI4", "Tr1", "Tr2"};

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

    size_t bracket_start = input.find('[');
    size_t bracket_end = input.find(']');
    if (bracket_start != std::string::npos && bracket_end != std::string::npos && bracket_end > bracket_start)
    {
        std::string tag = input.substr(bracket_start + 1, bracket_end - bracket_start - 1);
        res.emplace_back(tag);
    }

    return res;
}

std::vector<std::string> parse_insert(const std::string &input)
{
    size_t start = input.find_first_of("{"),
           end = input.find_last_of("}");
    string query = input.substr(start + 1, end - start - 1);
    regex token_regex("(?:\".*\"|[^[:space:]])+");
    vector<string> terms = regex_tokenizer(query, token_regex);

    return {terms[0], terms[1], terms[2]};
}

std::vector<std::string> parse_delete_edge(const std::string &input)
{
    size_t start = input.find_first_of("{"),
           end = input.find_last_of("}");
    string query = input.substr(start + 1, end - start - 1);
    regex token_regex("(?:\".*\"|[^[:space:]])+");
    vector<string> terms = regex_tokenizer(query, token_regex);

    return {terms[0], terms[1], terms[2]};
}

std::vector<std::string> parse_delete_node(const std::string &input)
{
    size_t start = input.find("?s=") + 3;
    size_t end = input.find_first_of(")", start);

    return {input.substr(start, end - start)};
}

std::pair<QueryType, std::vector<std::string>> parse_query(const std::string &input)
{
    std::istringstream iss(input);
    std::string word;
    iss >> word;

    if (word == "SELECT")
    {
        return {QueryType::SELECT, parse_select(input)};
    } 
    else if (word == "INSERT")
    {
        return {QueryType::INSERT, parse_insert(input)};
    }
    else if (word == "DELETE")
    {
        iss >> word;
        if (word == "WHERE")
        {
            return {QueryType::DELETE_EDGE, parse_delete_edge(input)};
        }
        else if (input.find("?s=") != std::string::npos)
        {
            return {QueryType::DELETE_NODE, parse_delete_node(input)};
        }
    }

    throw std::runtime_error("parse_query: consulta no reconocida: " + input);
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

template <class ring_type, class map_type>
void query_select(ring_type &graph, map_type &so_mapping, map_type &p_mapping, vector<string> &tokens_query, const uint64_t nQ)
{
    chrono::high_resolution_clock::time_point start, stop;
    double total_time = 0.0, forward_trad = 0.0, backward_trad = 0.0;
    chrono::duration<double> time_span;
    unordered_map<string, uint8_t> hash_table_vars;
    vector<ring::triple_pattern> query;

    // tipo de query select
    std::string type = tokens_query.back();
    if (select_type.count(type)) {
        tokens_query.pop_back();
    }

    start = chrono::high_resolution_clock::now();
    for (string &token : tokens_query)
    {
        auto triple_pattern = get_user_triple<map_type>(token, hash_table_vars, so_mapping, p_mapping);
        query.push_back(triple_pattern);
    }

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    forward_trad = time_span.count();

    start = chrono::high_resolution_clock::now();

    ring::ltj_algorithm<ring_type> ltj(&query, &graph);

    typedef vector<typename ring::ltj_algorithm<>::tuple_type> results_type;
    results_type res;
    ltj.join(res, 1000, 600);

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    total_time = time_span.count();

    start = chrono::high_resolution_clock::now();

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

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    backward_trad = time_span.count();

    cout << "Select;" << type << ";" <<  nQ << ";" << res.size();
    cout << ";" << (unsigned long long)(total_time * 1000000000ULL);
    cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
    cout << ";" << (unsigned long long)(backward_trad * 1000000000ULL) << endl;
}

template <class ring_type, class map_type>
void query_insert(ring_type &graph, map_type &so_mapping, map_type &p_mapping, vector<string> &tokens_query, const uint64_t nQ)
{
    chrono::high_resolution_clock::time_point start, stop;
    double total_time = 0.0, forward_trad = 0.0;
    chrono::duration<double> time_span;

    start = chrono::high_resolution_clock::now();

    spo_triple query_triple = {so_mapping.get_or_insert(tokens_query[0]), p_mapping.get_or_insert(tokens_query[1]), so_mapping.get_or_insert(tokens_query[2])};
    
    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    forward_trad = time_span.count();

    start = chrono::high_resolution_clock::now();

    graph.insert(query_triple);

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    total_time = time_span.count();

    cout << "Insert;" <<  nQ << ";" << (unsigned long long)(total_time * 1000000000ULL);
    cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL) << endl;
}

template <class ring_type, class map_type>
void query_delete_edge(ring_type &graph, map_type &so_mapping, map_type &p_mapping, vector<string> &tokens_query, const uint64_t nQ)
{
    chrono::high_resolution_clock::time_point start, stop;
    double total_time = 0.0, forward_trad = 0.0, backward_time = 0.0;
    chrono::duration<double> time_span;

    start = chrono::high_resolution_clock::now();

    std::pair<bool, uint64_t> so_id = so_mapping.locate(tokens_query[0]);
    std::pair<bool, uint64_t> p_id = p_mapping.locate(tokens_query[1]);
    std::pair<bool, uint64_t> so_id2 = so_mapping.locate(tokens_query[2]);

    // The node or the edge doesn't exist  in the dictionary
    if (! so_id.first || ! p_id.first || ! so_id2.first)
    {
        cout << "Edge doesn't exist;" <<  nQ << ";0";
        cout << ";" << (unsigned long long)(total_time * 1000000000ULL);
        cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
        cout << ";" << (unsigned long long)(backward_time * 1000000000ULL) << endl;
        return;
    }

    spo_triple query_triple = {so_id.second, p_id.second, so_id2.second};

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    forward_trad = time_span.count();

    start = chrono::high_resolution_clock::now();

    spo_valid_triple valid = graph.remove_edge_and_check(query_triple);

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    total_time = time_span.count();

    start = chrono::high_resolution_clock::now();

    // The given triple doesn't exist in the graph
    if (!get<3>(valid))
    {
        cout << "Edge doesn't exist;" << nQ << ";0";
        cout << ";" << (unsigned long long)(total_time * 1000000000ULL);
        cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
        cout << ";" << (unsigned long long)(backward_time * 1000000000ULL) << endl;
        return;
    }

    // Is S still in use? If not delete it from the mapping
    if (!get<0>(valid))
    {
        so_mapping.eliminate(get<0>(query_triple));
    }

    // Is P still in use? If not delete it from the mapping
    if (!get<1>(valid))
    {
        p_mapping.eliminate(get<1>(query_triple));
    }

    // Is O still in use? If not delete it from the mapping
    if (!get<2>(valid))
    {
        so_mapping.eliminate(get<2>(query_triple));
    }

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    backward_time = time_span.count();

    cout << "Delete edge;" <<  nQ << ";" << "1;";
    cout << (unsigned long long)(total_time * 1000000000ULL);
    cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
    cout << ";" << (unsigned long long)(backward_time * 1000000000ULL) << endl;
}

template <class ring_type, class map_type>
void query_delete_node(ring_type &graph, map_type &so_mapping, map_type &p_mapping, vector<string> &tokens_query, const uint64_t nQ)
{
    uint64_t node_id = 0;
    chrono::high_resolution_clock::time_point start, stop;
    double total_time = 0.0, forward_trad = 0.0, backward_time = 0.0;
    chrono::duration<double> time_span;

    start = chrono::high_resolution_clock::now();

    try {
        node_id = so_mapping.eliminate(tokens_query[0]);
    } catch (const std::invalid_argument& e) {
        cout << "Node doesn't exist;" << nQ << ";" << 0;
        cout << ";" << (unsigned long long)(total_time * 1000000000ULL);
        cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
        cout << ";" << (unsigned long long)(backward_time * 1000000000ULL) << endl;
        return;
    }

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    forward_trad = time_span.count();

    vector<uint64_t> so_removed_ids;
    vector<uint64_t> p_removed_ids;

    start = chrono::high_resolution_clock::now();

    uint64_t total_removed = graph.remove_node_with_check(node_id, so_removed_ids, p_removed_ids);

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    total_time = time_span.count();

    start = chrono::high_resolution_clock::now();

    for (uint64_t id : so_removed_ids)
    {
        so_mapping.eliminate(id);
    }

    for (uint64_t id : p_removed_ids)
    {
        p_mapping.eliminate(id);
    }

    stop = chrono::high_resolution_clock::now();
    time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
    backward_time = time_span.count();

    cout << "Delete node;" << nQ << ";" << total_removed << ";" << (unsigned long long)(total_time * 1000000000ULL);
    cout << ";" << (unsigned long long)(forward_trad * 1000000000ULL);
    cout << ";" << (unsigned long long)(backward_time * 1000000000ULL) << endl;
}

template <class ring_type, class map_type>
void mapped_ring(const string &file, const string &so_mapping_file, const string &p_mapping_file, const string &queries)
{
    vector<string> dummy_queries;

    bool result = get_file_content(queries, dummy_queries);

    map_type so_mapping;
    ifstream so_infs(so_mapping_file, ios::binary | ios::in);
    so_mapping.load(so_infs);

    cout << endl
         << " SO Mapping loaded " << so_mapping.bit_size() / 8 << " bytes" << endl;

    map_type p_mapping;
    ifstream p_infs(p_mapping_file, ios::binary | ios::in);
    p_mapping.load(p_infs);

    cout << endl
         << " P Mapping loaded " << p_mapping.bit_size() / 8 << " bytes" << endl;

    ring_type graph;

    cout << " Loading the index...";
    fflush(stdout);
    sdsl::load_from_file(graph, file);

    cout << endl
        << " Index loaded " << sdsl::size_in_bytes(graph) << " bytes" << endl;

    ifstream ifs;
    uint64_t nQ = 0;

    chrono::high_resolution_clock::time_point start, stop;
    double total_time = 0.0, forward_trad = 0.0, backward_trad = 0.0;
    chrono::duration<double> time_span;

    if (result)
    {
        for (string &query_string : dummy_queries)
        {
            if (nQ % 10 == 0) {
                std::cout << "Bit size;" << nQ/10 << ";" << sdsl::size_in_bytes(graph) << ";";
                std::cout << graph.bit_size() / 8 << ";" << so_mapping.bit_size() / 8;
                std::cout << ";" << p_mapping.bit_size() / 8 << std::endl;
            }

            pair<QueryType, vector<string>> tokens_query = parse_query(query_string);

            if (tokens_query.first == QueryType::SELECT)
            {
                query_select<ring_type, map_type>(graph, so_mapping, p_mapping, tokens_query.second, nQ);
            }
            else if (tokens_query.first == QueryType::INSERT)
            {
                query_insert<ring_type, map_type>(graph, so_mapping, p_mapping, tokens_query.second, nQ);
            }
            else if (tokens_query.first == QueryType::DELETE_EDGE)
            {
                query_delete_edge<ring_type, map_type>(graph, so_mapping, p_mapping, tokens_query.second, nQ);
            }
            else if (tokens_query.first == QueryType::DELETE_NODE)
            {
                query_delete_node<ring_type, map_type>(graph, so_mapping, p_mapping, tokens_query.second, nQ);
            }
            else
            {
                cout << "Query no reconocida" << endl;
            }
            nQ++;
        }
    }
}

string get_name(const string &path)
{
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = path.substr(lastSlash + 1);

    size_t lastDot = filename.find_last_of('.');
    std::string filenameFinal = filename.substr(0, lastDot);

    return filenameFinal;
}

int main(int argc, char *argv[])
{
    if (argc != 5 && argc != 6)
    {
        std::cout << "Usage: " << argv[0] << " <index> <queries> <SO mapping> <P mapping> [Theta]" << std::endl;
        return 0;
    }

    std::string index = argv[1];
    std::string queries = argv[2];
    std::string so_mapping = argv[3];
    std::string p_mapping = argv[4];
    std::string type = get_name(index);

    if (type == "ring-map")
    {
        mapped_ring<ring::ring<>, ring::basic_map>(index, so_mapping, p_mapping, queries);
    }
    else if (type == "ring-dyn-basic")
    {
        mapped_ring<ring::ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
    }
    else if (type == "ring-dyn-map")
    {
        mapped_ring<ring::medium_ring_dyn, ring::basic_map>(index, so_mapping, p_mapping, queries);
    }
    else if (type == "ring-dyn-map-avl")
    {
        mapped_ring<ring::medium_ring_dyn, ring::basic_map_avl>(index, so_mapping, p_mapping, queries);
    }
    else if (type == "ring-dyn-amo-map")
    {
        if (argc == 6) {
            amo::setTheta(std::stof(argv[5]));
        }
        mapped_ring<ring::ring_dyn_amo, ring::basic_map>(index, so_mapping, p_mapping, queries);
    }
    else if (type == "ring-dyn-amo-map-avl")
    {
        if (argc == 6) {
            amo::setTheta(std::stof(argv[5]));
        }
        mapped_ring<ring::ring_dyn_amo, ring::basic_map_avl>(index, so_mapping, p_mapping, queries);
    }
    else
    {
        std::cout << "Type of index: " << type << " is not supported." << std::endl;
    }

    return 0;
}
