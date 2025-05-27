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
    std::cout << "Tamaño: " << BV->size() << std::endl;
    std::cout << "Primer elemento (1): " << BV->at(0) << std::endl;
    std::cout << "Segundo elemento (0): " << BV->at(1) << std::endl;
    std::cout << "Tercero elemento (1): " << BV->at(2) << std::endl;
    std::cout << "Cuarto elemento (1): " << BV->at(3) << std::endl;
    std::cout << "Quinto elemento (0): " << BV->at(4) << std::endl;

    std::cout << "rank (2): " << BV->rank(4) << std::endl;
    std::cout << "rank_0 (2): " << BV->rank(4, 0) << std::endl;
    std::cout << "rank_1 (3): " << BV->rank(4, 1) << std::endl;

    std::cout << "select (4): " << BV->select(2, 0) << std::endl;
    std::cout << "select_0 (4): " << BV->select0(2) << std::endl;
    std::cout << "select (3): " << BV->select(3, 1) << std::endl;
    std::cout << "select_1 (3): " << BV->select1(3) << std::endl;

    std::cout << "Se remueven los ultimos 2" << std::endl;
    BV->remove(4);
    BV->remove(3);
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
}