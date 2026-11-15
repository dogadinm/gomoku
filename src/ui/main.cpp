#include "ui/interface.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>
using namespace gomoku;
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "--bench") == 0)
            return benchmark(2);
        if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
            std::cout << "Gomoku: 19x19 capture Gomoku\nUsage: ./Gomoku [--help | --bench]\nRun in "
                         "a 76x28 or larger terminal. Arrow keys and Space place stones.\n";
            return 0;
        }
        if (argc != 1) {
            std::cerr << "Unknown option; use --help.\n";
            return 1;
        }
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || !std::getenv("TERM")) {
            std::cerr << "Interactive mode needs a terminal; use --help or --bench.\n";
            return 1;
        }
        return run();
    } catch (const std::exception &e) {
        std::cerr << "Cannot continue: " << e.what() << '\n';
        return 1;
    }
}
