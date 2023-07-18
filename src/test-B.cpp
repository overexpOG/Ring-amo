#include "ring.hpp"
#include "triple_pattern.hpp"
#include "ltj_algorithm.hpp"
#include <dynamic/dynamic.hpp>
#include <chrono>
#include <iostream>

using namespace std;

// Parse Query

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

std::string ltrim(const string &s)
{
  size_t start = s.find_first_not_of(' ');
  return (start == string::npos) ? "" : s.substr(start);
}

string rtrim(const string &s)
{
  size_t end = s.find_last_not_of(' ');
  return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string trim(const string &s)
{
  return rtrim(ltrim(s));
}

vector<string> tokenizer(const string &input, const char &delimiter)
{
  stringstream stream(input);
  string token;
  vector<string> res;
  while (getline(stream, token, delimiter))
  {
    res.emplace_back(trim(token));
  }
  return res;
}

bool is_variable(string &s)
{
  return (s.at(0) == '?');
}

uint8_t get_variable(string &s, unordered_map<string, uint8_t> &hash_table_vars)
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
  return stoull(s);
}

ring::triple_pattern get_triple(string &s, unordered_map<string, uint8_t> &hash_table_vars)
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

// -- END PARSE QUERY --

template <class ring_type>
void test_ring_all(
    vector<spo_triple> &D, vector<vector<ring::triple_pattern>> &queries, vector<spo_triple> &insert_triples, vector<uint64_t> &node_delete,
    uint64_t b, uint64_t b_leaf)
{
  typedef vector<typename ring::ltj_algorithm<>::tuple_type> results_type;
  chrono::high_resolution_clock::time_point start, stop;
  double build_time = 0.0, query_time = 0.0, insert_time = 0.0, remove_edge_time = 0.0, remove_node_time = 0.0;
  uint64_t build_size = 0;
  chrono::duration<double> time_span;

  //! BUILDING
  start = chrono::high_resolution_clock::now();
  ring_type A(D);

  stop = chrono::high_resolution_clock::now();
  time_span = chrono::duration_cast<chrono::seconds>(stop - start);
  build_time = time_span.count();
  build_size = A.bit_size() / 8;

  //! QUERYING
  start = chrono::high_resolution_clock::now();
  for (vector<ring::triple_pattern> query : queries)
  {
    results_type res;

    ring::ltj_algorithm<ring_type> ltj(&query, &A);

    ltj.join(res, 1000, 600);
  }

  stop = chrono::high_resolution_clock::now();
  time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
  query_time = time_span.count();

  //! DELETE
  start = chrono::high_resolution_clock::now();

  for (spo_triple x : insert_triples)
  {
    A.remove_edge(x);
  }

  stop = chrono::high_resolution_clock::now();
  time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
  remove_edge_time = time_span.count();

  //! INSERT
  start = chrono::high_resolution_clock::now();

  for (spo_triple x : insert_triples)
  {
    A.insert(x);
  }

  stop = chrono::high_resolution_clock::now();
  time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
  insert_time = time_span.count();

  //! REMOVE NODE
  start = chrono::high_resolution_clock::now();

  for (uint64_t x : node_delete)
  {
    A.remove_node(x);
  }

  stop = chrono::high_resolution_clock::now();
  time_span = chrono::duration_cast<chrono::microseconds>(stop - start);
  remove_node_time = time_span.count();

  cout << b << ", " << b_leaf << ", " << build_time << ", " << build_size << ", ";
  cout << (unsigned long long)(query_time * 1000000000ULL) << ", ";
  cout << (unsigned long long)(insert_time * 1000000000ULL);
  cout << ", " << (unsigned long long)(remove_edge_time * 1000000000ULL) << ", ";
  cout << (unsigned long long)(remove_node_time * 1000000000ULL) << endl;
}

void instantiate_and_run()
{
  vector<spo_triple> D, insert_triples;
  vector<uint64_t> nodes;
  vector<vector<ring::triple_pattern>> queries;
  vector<string> lines;

  // Important files to read to
  ifstream ifs("../data/dump-test.dat");
  ifstream triples_file("../Queries/B-tests/insertDelete.txt");
  ifstream nodes_file("../Queries/B-tests/deleteNode.txt");

  // All of the triples
  uint64_t s, p, o;
  do
  {
    ifs >> s >> p >> o;
    D.push_back(spo_triple(s, p, o));
  } while (!ifs.eof());

  // Insert and delete triples
  do
  {
    triples_file >> s >> p >> o;
    insert_triples.push_back(spo_triple(s, p, o));
  } while (!triples_file.eof());

  // Nodes values
  do
  {
    nodes_file >> s;
    nodes.push_back(s);
  } while (!nodes_file.eof());

  // Queries
  get_file_content("../Queries/B-tests/queries-test.txt", lines);
  for (string &query_string : lines)
  {
    std::unordered_map<std::string, uint8_t> hash_table_vars;
    std::vector<ring::triple_pattern> query;
    vector<string> tokens_query = tokenizer(query_string, '.');
    for (string &token : tokens_query)
    {
      auto triple_pattern = get_triple(token, hash_table_vars);
      query.push_back(triple_pattern);
    }
    queries.push_back(query);
  }

  insert_triples.shrink_to_fit();
  nodes.shrink_to_fit();
  D.shrink_to_fit();
  queries.shrink_to_fit();

  typedef dyn::wm_string<dyn::succinct_bitvector<dyn::spsi<dyn::packed_bit_vector, 256, 64>>> big_wm_str_1;
  typedef dyn::wm_string<dyn::succinct_bitvector<dyn::spsi<dyn::packed_bit_vector, 512, 64>>> big_wm_str_2;
  typedef dyn::wm_string<dyn::succinct_bitvector<dyn::spsi<dyn::packed_bit_vector, 1024, 64>>> big_wm_str_3;
  typedef dyn::wm_string<dyn::succinct_bitvector<dyn::spsi<dyn::packed_bit_vector, 2048, 64>>> big_wm_str_4;
  typedef dyn::wm_string<dyn::succinct_bitvector<dyn::spsi<dyn::packed_bit_vector, 4096, 64>>> big_wm_str_5;
  typedef dyn::wm_string<dyn::succinct_bitvector<dyn::spsi<dyn::packed_bit_vector, 8192, 64>>> big_wm_str_6;

  test_ring_all<ring::ring<ring::bwt_dyn<dyn::suc_bv, big_wm_str_1>, ring::bwt_dyn<dyn::suc_bv, big_wm_str_1>>>(D, queries, insert_triples, nodes, 64, 256);
  test_ring_all<ring::ring<ring::bwt_dyn<dyn::suc_bv, big_wm_str_2>, ring::bwt_dyn<dyn::suc_bv, big_wm_str_2>>>(D, queries, insert_triples, nodes, 64, 512);
  test_ring_all<ring::ring<ring::bwt_dyn<dyn::suc_bv, big_wm_str_3>, ring::bwt_dyn<dyn::suc_bv, big_wm_str_3>>>(D, queries, insert_triples, nodes, 64, 1024);
  test_ring_all<ring::ring<ring::bwt_dyn<dyn::suc_bv, big_wm_str_4>, ring::bwt_dyn<dyn::suc_bv, big_wm_str_4>>>(D, queries, insert_triples, nodes, 64, 2048);
  test_ring_all<ring::ring<ring::bwt_dyn<dyn::suc_bv, big_wm_str_5>, ring::bwt_dyn<dyn::suc_bv, big_wm_str_5>>>(D, queries, insert_triples, nodes, 64, 4096);
  test_ring_all<ring::ring<ring::bwt_dyn<dyn::suc_bv, big_wm_str_6>, ring::bwt_dyn<dyn::suc_bv, big_wm_str_6>>>(D, queries, insert_triples, nodes, 64, 8192);
}

int main()
{
  instantiate_and_run();
}