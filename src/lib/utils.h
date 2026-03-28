#pragma once

#include <iostream>

template<typename ...Args>
void log(Args&&... args) {
    int i = 0;
    ((std::cout << (i++ ? " " : "") << args), ...);
    std::cout << "\n";
}

template<typename ...Args>
void logErr(Args&&... args) {
    int i = 0;
    std::cerr << "ERROR: ";
    ((std::cerr << (i++ ? " " : "") << args), ...);
    std::cerr << "\n";
}
