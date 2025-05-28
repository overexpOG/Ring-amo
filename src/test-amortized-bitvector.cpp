#include "bitvector_amortized/hybrid.hpp"

int main() {
    std::cout << "Se crea el bitvector" << std::endl;
    amo::HybridBV *BV = new amo::HybridBV();
    std::cout << "Tamaño: " << BV->size() << std::endl;

    std::cout << "===============================" << std::endl;
    std::cout << "             LEAF              " << std::endl;
    std::cout << "===============================" << std::endl;

    std::cout << "Se inserta 10110" << std::endl;
    BV->push_back(1);
    BV->insert0(1);
    BV->insert1(2);
    BV->insert(3, 1);
    BV->insert(4,0);
    std::cout << "Tamaño (5): " << BV->size() << std::endl;
    std::cout << "Tipo (LeafBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (1): " << BV->leaves() << std::endl;
    std::cout << "Primer elemento (1): " << BV->at(0) << std::endl;
    std::cout << "Segundo elemento (0): " << BV->at(1) << std::endl;
    std::cout << "Tercero elemento (1): " << BV->at(2) << std::endl;
    std::cout << "Cuarto elemento (1): " << BV->at(3) << std::endl;
    std::cout << "Quinto elemento (0): " << BV->at(4) << std::endl;

    std::cout << "rank (3): " << BV->rank(4) << std::endl;
    std::cout << "rank_0 (2): " << BV->rank(4, 0) << std::endl;
    std::cout << "rank_1 (3): " << BV->rank(4, 1) << std::endl;

    std::cout << "select (4): " << BV->select(2, 0) << std::endl;
    std::cout << "select_0 (4): " << BV->select0(2) << std::endl;
    std::cout << "select (3): " << BV->select(3, 1) << std::endl;
    std::cout << "select_1 (3): " << BV->select1(3) << std::endl;

    std::cout << "Se remueven los ultimos 2" << std::endl;
    BV->remove(4);
    BV->remove(3);
    std::cout << "Tamaño (3): " << BV->size() << std::endl;
    std::cout << "Primer elemento (1): " << BV->at(0) << std::endl;
    std::cout << "Segundo elemento (0): " << BV->at(1) << std::endl;
    std::cout << "Tercero elemento (1): " << BV->at(2) << std::endl;

    std::cout << "Se setea en el contrario" << std::endl;
    BV->set(0, 0);
    BV->set(1, 1);
    BV->set(2, 0);
    std::cout << "Primer elemento (0): " << BV->at(0) << std::endl;
    std::cout << "Segundo elemento (1): " << BV->at(1) << std::endl;
    std::cout << "Tercero elemento (0): " << BV->at(2) << std::endl;

    std::cout << "Se elimina el bitvector" << std::endl;
    delete BV;

    std::cout << "===============================" << std::endl;
    std::cout << "            DYNAMIC            " << std::endl;
    std::cout << "===============================" << std::endl;

    std::cout << "Se crea el bitvector" << std::endl;
    BV = new amo::HybridBV();
    std::cout << "Se inserta el limite de un leaf" << std::endl;
    for (int i = 0; i < 32 * 64; i++) {
        BV->push_back(0);
    }
    std::cout << "Tamaño (2048): " << BV->size() << std::endl;
    std::cout << "Tipo (LeafBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (1): " << BV->leaves() << std::endl;

    std::cout << "Se inserta 10110" << std::endl;
    BV->push_back(1);
    BV->insert0(32*64 + 1);
    BV->insert1(32*64 + 2);
    BV->insert(32*64 + 3, 1);
    BV->insert(32*64 + 4, 0);
    std::cout << "Tamaño (2053): " << BV->size() << std::endl;
    std::cout << "Tipo (DynamicBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (2): " << BV->leaves() << std::endl;
    std::cout << "Primer elemento (1): " << BV->at(32*64 + 0) << std::endl;
    std::cout << "Segundo elemento (0): " << BV->at(32*64 + 1) << std::endl;
    std::cout << "Tercero elemento (1): " << BV->at(32*64 + 2) << std::endl;
    std::cout << "Cuarto elemento (1): " << BV->at(32*64 + 3) << std::endl;
    std::cout << "Quinto elemento (0): " << BV->at(32*64 + 4) << std::endl;

    std::cout << "rank (3): " << BV->rank(32*64 + 4) << std::endl;
    std::cout << "rank_0 (2050): " << BV->rank(32*64 + 4, 0) << std::endl;
    std::cout << "rank_1 (3): " << BV->rank(32*64 + 4, 1) << std::endl;

    std::cout << "select (2052): " << BV->select(2050, 0) << std::endl;
    std::cout << "select_0 (2052): " << BV->select0(2050) << std::endl;
    std::cout << "select (2051): " << BV->select(3, 1) << std::endl;
    std::cout << "select_1 (2051): " << BV->select1(3) << std::endl;

    std::cout << "Se remueven el primero y ultimo" << std::endl;
    BV->remove(0);
    BV->remove(32*64 + 4);

    std::cout << "Tamaño (2051): " << BV->size() << std::endl;
    std::cout << "Cero elemento (1): " << BV->at(32*64 - 1) << std::endl;
    std::cout << "Primer elemento (0): " << BV->at(32*64 + 0) << std::endl;
    std::cout << "Segundo elemento (1): " << BV->at(32*64 + 1) << std::endl;
    std::cout << "Tercero elemento (1): " << BV->at(32*64 + 2) << std::endl;

    std::cout << "Se setea en el contrario" << std::endl;
    BV->set(32*64 - 1, 0);
    BV->set(32*64 + 0, 1);
    BV->set(32*64 + 1, 0);
    BV->set(32*64 + 2, 0);
    std::cout << "Cero elemento (0): " << BV->at(32*64 - 1) << std::endl;
    std::cout << "Primer elemento (1): " << BV->at(32*64 + 0) << std::endl;
    std::cout << "Segundo elemento (0): " << BV->at(32*64 + 1) << std::endl;
    std::cout << "Tercero elemento (0): " << BV->at(32*64 + 2) << std::endl;

    std::cout << "Se elimina el bitvector" << std::endl;
    delete BV;

    std::cout << "===============================" << std::endl;
    std::cout << "            STATIC            " << std::endl;
    std::cout << "===============================" << std::endl;

    std::cout << "Se crea el bitvector" << std::endl;
    BV = new amo::HybridBV();
    std::cout << "Se insertan dos leaf al limite" << std::endl;
    for (int i = 0; i < 32 * 64; i++) {
        BV->insert(0, i % 2);
        BV->push_back(i % 2);
    }
    std::cout << "Tamaño (4096): " << BV->size() << std::endl;
    std::cout << "Tipo (DynamicBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (2): " << BV->leaves() << std::endl;

    std::cout << "Primer elemento (1): " << BV->at(0) << std::endl;
    std::cout << "Segundo elemento (0): " << BV->at(1) << std::endl;
    std::cout << "Tercero elemento (1): " << BV->at(2) << std::endl;
    std::cout << "Cuarto elemento (0): " << BV->at(3) << std::endl;
    std::cout << "Quinto elemento (1): " << BV->at(4) << std::endl;

    std::cout << "Se transforma en static accediendo a todos sus elementos" << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }

    std::cout << "Tamaño (4096): " << BV->size() << std::endl;
    std::cout << "Tipo (StaticBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (3?): " << BV->leaves() << std::endl;

    std::cout << "Primer elemento (1): " << BV->at(0) << std::endl;
    std::cout << "Segundo elemento (0): " << BV->at(1) << std::endl;
    std::cout << "Tercero elemento (1): " << BV->at(2) << std::endl;
    std::cout << "Cuarto elemento (0): " << BV->at(3) << std::endl;
    std::cout << "Quinto elemento (1): " << BV->at(4) << std::endl;

    std::cout << "rank (3): " << BV->rank(4) << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "rank_0 (2): " << BV->rank(4, 0) << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "rank_1 (3): " << BV->rank(4, 1) << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "select (3): " << BV->select(2, 0) << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "select_0 (3): " << BV->select0(2) << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "select (4): " << BV->select(3, 1) << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "select_1 (4): " << BV->select1(3) << std::endl;

    std::cout << "Se remueven el primero elemento" << std::endl;
    BV->remove(0);
    std::cout << "Tamaño (4095): " << BV->size() << std::endl;
    std::cout << "Tipo (DynamicBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (3?): " << BV->leaves() << std::endl;
    std::cout << "Se transforma en static" << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "Tamaño (4095): " << BV->size() << std::endl;
    std::cout << "Tipo (StaticBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (3?): " << BV->leaves() << std::endl;

    std::cout << "Se setea el primer elemento en el contrario" << std::endl;
    BV->set(0, 1);
    std::cout << "Tamaño (4095): " << BV->size() << std::endl;
    std::cout << "Tipo (DynamicBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (3?): " << BV->leaves() << std::endl;
    std::cout << "Se transforma en static" << std::endl;
    for (int i = 0; i < 2 * 32 * 64; i++) {
        BV->at(i);
    }
    std::cout << "Tamaño (4095): " << BV->size() << std::endl;
    std::cout << "Tipo (StaticBV): " << BV->getType() << std::endl;
    std::cout << "Leaves (3?): " << BV->leaves() << std::endl;

    std::cout << "Primer elemento (1): " << BV->at(0) << std::endl;
    std::cout << "Segundo elemento (1): " << BV->at(1) << std::endl;
    std::cout << "Tercero elemento (0): " << BV->at(2) << std::endl;
    std::cout << "Cuarto elemento (1): " << BV->at(3) << std::endl;
    std::cout << "Quinto elemento (0): " << BV->at(4) << std::endl;
}