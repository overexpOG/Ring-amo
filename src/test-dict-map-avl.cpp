#include "dict_map_avl.hpp"
#include "dict_map.hpp"
#include <string>

// Función para verificar si un número es primo
bool esPrimo(int n) {
    if (n < 2) return false;
    int raiz = sqrt(n);
    for (int i = 2; i <= raiz; ++i) {
        if (n % i == 0) return false;
    }
    return true;
}

// Genera los primeros `cantidad` primos
vector<int> generarPrimos(int cantidad) {
    vector<int> primos;
    int num = 2;
    while (primos.size() < cantidad) {
        if (esPrimo(num)) {
            primos.push_back(num);
        }
        ++num;
    }
    return primos;
}

int main() {
    unsigned int n = 1024;
    std::cout << "Se crean los diccionarios" << std::endl;
    ring::basic_map *dict = new ring::basic_map();
    ring::basic_map_avl *dict_avl = new ring::basic_map_avl();
    std::cout << "Tamaño: " << dict->size() << std::endl;
    std::cout << "Tamaño avl: " << dict_avl->size() << std::endl;

    vector<int> primos = generarPrimos(n+128);

    for (unsigned int j = 0; j < n; j++) {
        std::cout << "Insertar 128 elementos (nodo " << j + 1 << "): " << std::endl;
        for (unsigned int i = 1; i < 128; i++) {
            std::string s = "https://" + std::to_string(primos[j] * primos[i+n]);
            dict->insert(s);
            dict_avl->insert(s);
        }

        std::cout << "Tamaño: " << dict->size() << std::endl;
        std::cout << "Altura: " << dict->get_height() << std::endl;
        std::cout << "Tamaño avl: " << dict_avl->size() << std::endl;
        std::cout << "Altura avl: " << dict_avl->get_height() << std::endl;

    }

    std::cout << "Espacio: " << dict->bit_size() << std::endl;
    std::cout << "Espacio AVL: " << dict_avl->bit_size() << std::endl;

    return 0;
}