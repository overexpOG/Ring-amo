/*
 * dict_map.cpp
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

#ifndef DICT_MAP_HPP
#define DICT_MAP_HPP

#include "pfc.hpp"

namespace ring
{

  /**
   * @brief Dictionary that maps the user symbols to IDs used by the ring
   * It uses a binary tree with Plain Front Coding as the leaves.
   * It also has an array mapping every ID with its corresponding PFC
   *
   * @tparam MINSIZE The minimum words a PFC can have before being fused with its sibling
   * @tparam MAXSIZE The maximum words a PFC can have before splitting
   */
  template <uint64_t MINSIZE, uint64_t MAXSIZE>
  class dict_map
  {
  public:
    dict_map()
    {
      root = new node();
    }

    dict_map(std::string val)
    {
      root = new node(val, 1);
      id_map.push_back({ .pfc = root->get_pfc()});
    }

    // Move constructor
    dict_map(dict_map &&o)
    {
      *this = std::move(o);
    }

    ~dict_map()
    {
      root->free_mem();
      delete root;
    }

    /**
     * @brief Function that serializes the data structure.
     *
     * @param out The out stream where the bytes are being written
     * @return uint64_t The amount of bytes written
     */
    uint64_t serialize(std::ostream &out)
    {
      uint64_t w_bytes = 0;
      size_t map_size = id_map.size();

      out.write((char *)&map_size, sizeof(map_size));
      w_bytes += sizeof(map_size);
      w_bytes += root->serialize(out);
      out.write((char *)&free_ids_size, sizeof(uint64_t));
      w_bytes += sizeof(uint64_t);
      out.write((char *)&first_empty, sizeof(uint64_t));
      w_bytes += sizeof(uint64_t);

      if (free_ids_size > 0)
      {
        // For every empty slot write the next empty
        for (uint64_t i = 0; i < free_ids_size - 1; i++)
        {
          out.write((char *)&id_map[first_empty - 1].next_empty, sizeof(uint64_t));
          first_empty = id_map[first_empty - 1].next_empty;
          w_bytes += sizeof(uint64_t);
        }
      }

      return w_bytes;
    }

    //! Serializes the data structure into the given ostream
    uint64_t serialize(ostream &out, sdsl::structure_tree_node *v = nullptr, string name = "") const
    {
      sdsl::structure_tree_node *child = sdsl::structure_tree::add_child(v, name, "dict_map");
      uint64_t written_bytes = 0;
      size_t map_size = id_map.size();

      written_bytes += sdsl::write_member(map_size, out, child, "map_size");
      written_bytes += root->serialize(out);
      written_bytes += sdsl::write_member(free_ids_size, out, child, "free_ids_size");
      written_bytes += sdsl::write_member(first_empty, out, child, "first_empty");
      if (free_ids_size > 0)
      {
        // For every empty slot write the next empty
        for (uint64_t i = 0; i < free_ids_size - 1; i++)
        {
          out.write((char *)&id_map[first_empty - 1].next_empty, sizeof(uint64_t));
          first_empty = id_map[first_empty - 1].next_empty;
          written_bytes += sizeof(uint64_t);
        }
      }
      sdsl::structure_tree::add_size(child, written_bytes);

      return written_bytes;
    }

    /**
     * @brief Loads a serialized Dict Map from a stream of bytes
     *
     * @param in The in stream where the bytes are coming from
     */
    void load(std::istream &in)
    {
      size_t map_size;

      sdsl::read_member(map_size, in);
      id_map = std::vector<EmptyOrPFC>(map_size);
      root = new node();
      root->load(in, id_map);
      sdsl::read_member(free_ids_size, in);
      sdsl::read_member(first_empty, in);
      uint64_t tmp = 0;
      last_empty = first_empty;
      if (free_ids_size > 0)
      {
        for (uint64_t i = 0; i < free_ids_size - 1; i++)
        {
          in.read((char *)&tmp, sizeof(uint64_t));
          id_map[last_empty - 1].next_empty = tmp;
          last_empty = tmp;
        }
      }
    }

    /**
     * @brief Inserts a value into the structure.
     * Assigns an ID and inserts it into the tree.
     *
     * @param val value being inserted
     *
     * @return The ID assigned to the inserted value
     */
    uint64_t insert(const std::string &val)
    {
      uint64_t id;
      if (free_ids_size == 0)
      {
        id = id_map.size() + 1;
        id_map.push_back({ .pfc = root->insert(val, id, id_map)});
      }
      else
      {
        id = first_empty;
        // Last element on "Symbolic queue"
        if (free_ids_size == 1)
        {
          last_empty = 0;
          first_empty = 0;
        } else {
          first_empty = id_map[id - 1].next_empty;
        }
        id_map[id - 1].pfc = root->insert(val, id, id_map);
        free_ids_size--;
      }

      return id;
    }

    /**
     * @brief Get the ID associated to the value or insert it if its not in the mapping
     *
     * @param val value being searched or inserted
     * @return uint64_t the id assigned to the value
     */
    uint64_t get_or_insert(const std::string &val)
    {
      uint64_t id, found_id;
      std::tuple<uint64_t, PFC *> res;
      if (free_ids_size == 0)
      {
        id = id_map.size() + 1;
        res = root->get_or_insert(val, id, id_map);
        found_id = std::get<0>(res);
        if (found_id == id)
        {
          id_map.push_back({ .pfc = std::get<1>(res) });
        }
      }
      else
      {
        id = first_empty;
        res = root->get_or_insert(val, id, id_map);
        found_id = std::get<0>(res);
        if (found_id == id)
        {
          // Last element on "Symbolic queue"
          if (free_ids_size == 1)
          {
            last_empty = 0;
            first_empty = 0;
          } else {
            first_empty = id_map[id - 1].next_empty;
          }
          id_map[id - 1].pfc = std::get<1>(res);
          free_ids_size--;
        }
      }

      return found_id;
    }

    spo_triple get_or_insert_triple(const std::string &s, const std::string &p, const std::string &o)
    {
      return {this->get_or_insert(s), this->get_or_insert(p), this->get_or_insert(o)};
    }

    /**
     * @brief Deletes a value from the mapping
     *
     * @param val value being eliminated
     *
     * @return The ID of the eliminated value
     */
    uint64_t eliminate(const std::string &val)
    {
      uint64_t elim_id = std::get<0>(root->eliminate(val, id_map));
      // First in "Symbolic queue"
      if (free_ids_size == 0)
      {
        first_empty = elim_id;
        id_map[elim_id - 1].pfc = nullptr;
      }
      else
      {
        id_map[last_empty - 1].next_empty = elim_id;
      }
      last_empty = elim_id;
      free_ids_size++;
      return elim_id;
    }

    /**
     * @brief Deletes a value from the mapping
     *
     * @param id ID of the value being eliminated
     */
    void eliminate(const uint64_t id)
    {
      id_map[id - 1].pfc->elim(id);
      id_map[id - 1].pfc = nullptr;
      // First in "Symbolic queue"
      if (free_ids_size == 0)
      {
        first_empty = id;
      }
      else
      {
        id_map[last_empty - 1].next_empty = id;
      }
      last_empty = id;
      free_ids_size++;
    }

    /**
     * @brief Search a value in the binary tree of the structure and get its ID
     *
     * @param val value being searched
     * @return uint64_t ID associated to the value
     */
    uint64_t locate(const std::string &val) const
    {
      return root->search(val);
    }

    /**
     * @brief Search for an ID in the structure and get its corresponding value
     *
     * @param id the ID being searched
     * @return std::string value associated to the ID
     */
    std::string extract(uint64_t id)
    {
      assert(id > 0 && id - 1 < id_map.size());
      return id_map[id - 1].pfc->extract(id);
    }

    size_t size()
    {
      return id_map.size();
    }

    size_t bit_size() const
    {
      size_t id_size = 8 * sizeof(id_map) + 8 * id_map.size() * sizeof(EmptyOrPFC);
      return 8 * sizeof(root) + id_size + root->bit_size();
    }

    std::string root_value()
    {
      return root->get_value();
    }

    PFC *get_root_pfc()
    {
      return root->get_pfc();
    }

    int get_height() const {
      return root ? root->get_height() : 0;
  }

  private:
    class node;
    node *root = NULL;
    std::vector<EmptyOrPFC> id_map;
    // Values used to represent the Queue of free IDs
    uint64_t first_empty = 0, last_empty = 0, free_ids_size = 0;
  };

  /**
   * @brief Class representing the node of the binary tree
   * in the dictionary mapping.
   *
   * @tparam MINSIZE The minimum words a PFC can have before being fused with its sibling
   * @tparam MAXSIZE The maximum words a PFC can have before splitting
   */
  template <uint64_t MINSIZE, uint64_t MAXSIZE>
  class dict_map<MINSIZE, MAXSIZE>::node
  {
  public:
    node()
    {
      _is_leaf = true;
      pfc = new PFC();
    }

    node(std::string val, uint64_t id)
    {
      _is_leaf = true;
      pfc = new PFC();
      pfc->insert(val, id);
    }

    node(PFC *p)
    {
      _is_leaf = true;
      pfc = p;
    }

    void free_mem()
    {
      if (_is_leaf)
      {
        delete pfc;
      }
      else
      {
        left->free_mem();
        right->free_mem();
        delete left;
        delete right;
      }
    }

    bool is_leaf() { return _is_leaf; }

    PFC *get_pfc()
    {
      return pfc;
    }

    size_t bit_size() const
    {
      size_t bs = 8 * sizeof(left) * 2;
      bs += 8 * sizeof(pfc);
      bs += 8 * sizeof(bool);
      if (_is_leaf)
      {
        return bs + pfc->bit_size();
      }
      else
      {
        return bs + left->bit_size() + right->bit_size();
      }
    }

    std::string get_value()
    {
      return pfc->first_word();
    }

    /**
     * @brief Function that serializes the node data structure.
     *
     * @param out The out stream where the bytes are being written
     * @return uint64_t The amount of bytes written
     */
    uint64_t serialize(std::ostream &out)
    {
      uint64_t w_bytes = 0;

      out.write((char *)&_is_leaf, sizeof(_is_leaf));
      w_bytes += sizeof(_is_leaf);

      if (_is_leaf)
      {
        w_bytes += pfc->serialize(out);
      }
      else
      {
        w_bytes += left->serialize(out);
        w_bytes += right->serialize(out);
      }

      return w_bytes;
    }

    /**
     * @brief Loads a stream of bytes into a node. It also points
     * every ID being stored in the leaf to that same leaf.
     *
     * @param in The in stream where the bytes are coming from
     * @param id_map Reference to the vector that maps every ID to its corresponding PFC
     * @return PFC* The pointer to the leftmost leaf in the node subtree
     */
    PFC *load(std::istream &in, std::vector<EmptyOrPFC> &id_map)
    {
      size_t string_size;
      in.read((char *)&_is_leaf, sizeof(_is_leaf));

      if (_is_leaf)
      {
        pfc = new PFC();
        pfc->load(in, id_map);
        return pfc;
      }
      else
      {
        left = new node();
        PFC *left_pfc = left->load(in, id_map);
        right = new node();
        pfc = right->load(in, id_map);
        return left_pfc;
      }
    }

    /**
     * @brief Inserts a value into the tree on its corresponding Plain Front Coding
     * It assumes that the value being inserted is not part of the dictionary
     *
     * @param val value being inserted
     * @param id ID assigned to that value
     */
    PFC *insert(const std::string &val, const uint64_t &id, std::vector<EmptyOrPFC> &id_map)
    {
      if (is_leaf())
      {
        // Insert in PFC
        pfc->insert(val, id);
        if (pfc->size() > MAXSIZE)
        {
          // Split PFC
          std::tuple<std::string, uint64_t> res = pfc->split();
          PFC *new_pfc = new PFC(std::get<0>(res), std::get<1>(res));
          _is_leaf = false;
          right = new node(new_pfc);
          left = new node(pfc);
          pfc = new_pfc;

          // Update ID mapping
          for (uint64_t id : pfc->all_ids())
          {
            id_map[id - 1].pfc = pfc;
          }

          if (val.compare(pfc->first_word()) >= 0)
            return right->get_pfc();
          else
            return left->get_pfc();
        }
        return pfc;
      }
      else
      {
        // Go to correct children
        if (val.compare(pfc->first_word()) > 0)
        {
          return right->insert(val, id, id_map);
        }
        else
        {
          return left->insert(val, id, id_map);
        }
      }
    }

    /**
     * @brief Get the ID associated to the value or insert it if its not in the mapping
     *
     * @param val value being searched or inserted
     * @param id  ID assigned to the value if its inserted
     * @return std::tuple<uint64_t, PFC *> a pair containing the ID of the value and the PFC it was found/inserted
     */
    std::tuple<uint64_t, PFC *> get_or_insert(const std::string &val, const uint64_t &id, std::vector<EmptyOrPFC> &id_map)
    {
      if (is_leaf())
      {
        // Search or insert in PFC
        uint64_t new_id = pfc->get_or_insert(val, id);
        if (pfc->size() > MAXSIZE)
        {
          // Split PFC
          std::tuple<std::string, uint64_t> res = pfc->split();
          PFC *new_pfc = new PFC(std::get<0>(res), std::get<1>(res));
          _is_leaf = false;
          right = new node(new_pfc);
          left = new node(pfc);
          pfc = new_pfc;

          // Update ID mapping
          for (uint64_t id : pfc->all_ids())
          {
            id_map[id - 1].pfc = pfc;
          }

          if (val.compare(pfc->first_word()) >= 0)
            return {new_id, right->get_pfc()};
          else
            return {new_id, left->get_pfc()};
        }
        return {new_id, pfc};
      }
      else
      {
        // Go to correct children
        int r = val.compare(pfc->first_word());
        if (r == 0)
        {
          uint64_t new_id = pfc->get_or_insert(val, id);
          return {new_id, pfc};
        }
        else if (r > 0)
        {
          return right->get_or_insert(val, id, id_map);
        }
        else
        {
          return left->get_or_insert(val, id, id_map);
        }
      }
    }

    /**
     * @brief Find and delete the value given
     *
     * @param val value being deleted
     * @return std::tuple<uint64_t, uint64_t> a pair containing
     * the ID of the deleted value and the resulting size of the PFC it was stored in
     */
    std::tuple<uint64_t, uint64_t> eliminate(const std::string &val, std::vector<EmptyOrPFC> &id_map)
    {
      if (is_leaf())
      {
        // Delete word in PFC
        return {pfc->elim(val), pfc->size()};
      }
      else
      {
        int r = val.compare(pfc->first_word());
        std::tuple<uint64_t, uint64_t> res;
        uint64_t child_size = MAXSIZE;
        // Go to correct children
        if (r == 0)
        {
          // Go to PFC
          res = right->eliminate(val, id_map);
          child_size = std::get<1>(res);
        }
        else if (r < 0)
        {
          res = left->eliminate(val, id_map);
          child_size = std::get<1>(res);
        }
        else
        {
          res = right->eliminate(val, id_map);
          child_size = std::get<1>(res);
        }

        if (child_size < MINSIZE)
        {
          // Update ID mapping
          for (uint64_t id : right->pfc->all_ids())
          {
            id_map[id - 1].pfc = left->pfc;
          }

          std::string right_string = right->pfc->pfc_string();

          left->pfc->fuse(right_string, right->pfc->size());
          right->free_mem();
          delete right;
          _is_leaf = true;
          pfc = left->pfc;
          left->pfc = nullptr;
          delete left;

          if (pfc->size() > MAXSIZE)
          {
            // Split PFC
            std::tuple<std::string, uint64_t> split_res = pfc->split();
            PFC *new_pfc = new PFC(std::get<0>(split_res), std::get<1>(split_res));
            _is_leaf = false;
            right = new node(new_pfc);
            left = new node(pfc);
            pfc = new_pfc;

            // Update ID mapping
            for (uint64_t id : pfc->all_ids())
            {
              id_map[id - 1].pfc = pfc;
            }
          }
        }

        return {std::get<0>(res), MAXSIZE};
      }
    }

    /**
     * @brief Search a value in the Binary Tree
     *
     * @param val value being searched
     * @return uint64_t ID associated to the value given
     */
    uint64_t search(const std::string &val)
    {
      if (is_leaf())
      {
        return pfc->locate(val);
      }
      else
      {
        int r = val.compare(pfc->first_word());
        if (r == 0)
        {
          return pfc->locate(val);
        }
        else if (r < 0)
        {
          return left->search(val);
        }
        else
        {
          return right->search(val);
        }
      }
    }

    int get_height() const {
      if (_is_leaf) {
        return 1;
      }
      return 1 + std::max(left->get_height(), right->get_height());
    }

  private:
    bool _is_leaf = false;
    node *left = NULL;
    node *right = NULL;
    PFC *pfc = NULL;
  };

  typedef dict_map<32, 128> basic_map;
}

#endif