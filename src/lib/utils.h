#pragma once

#include <filesystem>
#include <fstream>
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

static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        log("CWD: ",std::filesystem::current_path());
        logErr("Failed to open file:", path);
        throw std::runtime_error("");
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
