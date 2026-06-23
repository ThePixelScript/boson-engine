#include <iostream>
#include <string>
#include <sstream>

/**
 * Handles the baseline Universal Chess Interface protocol handshake loop.
 */
void handleUCICommand() {
    std::cout << "id name Boson 0.1.0\n";
    std::cout << "id author Divesh Soundar Pillai\n";
    std::cout << "uciok\n" << std::flush;
}

int main() {
    // Synchronize and untie C++ streams for peak performance execution
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string lineInput;
    while (std::getline(std::cin, lineInput)) {
        if (lineInput.empty()) continue;

        std::string primaryCommand;
        std::istringstream commandStream(lineInput);
        commandStream >> primaryCommand;

        if (primaryCommand == "uci") {
            handleUCICommand();
        } else if (primaryCommand == "isready") {
            std::cout << "readyok\n" << std::flush;
        } else if (primaryCommand == "quit") {
            break;
        }
    }

    return 0;
}