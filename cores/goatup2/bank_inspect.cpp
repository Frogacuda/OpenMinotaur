// Developer data inspection tool. No renderer or gameplay approximation.
#include "stage_bank.h"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: mino_goatup2_bank_inspect <GoatUp 2 resource directory>\n";
        return 1;
    }
    std::cout << "bank,level,width,height,seed,expanded_cells,objects,placements,backup_writes\n";
    for (int bank = 0; bank < 4; ++bank) {
        std::ifstream in(std::filesystem::u8path(argv[1]) / ("DEFAULT_" + std::to_string(bank)),
                         std::ios::binary);
        if (!in) {
            std::cerr << "Missing DEFAULT_" << bank
                      << "; supply a resource directory containing DEFAULT_0 through DEFAULT_3.\n";
            return 1;
        }
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
        goatup2::Bank data;
        std::string error;
        if (!goatup2::readBank(bytes, data, error)) {
            std::cerr << error << "\n";
            return 1;
        }
        for (int level = 0; level < 20; ++level) {
            const auto &s = data.stages[level];
            goatup2::PlacementTable table;
            table.load(s);
            int count = 0;
            for (const auto &p : table.entries())
                count += p.type >= 0;
            std::cout << bank << ',' << level << ',' << s.displayWidth << ',' << s.displayHeight
                      << ',' << s.seed << ',' << s.expandedCells << ',' << s.objects.size() << ','
                      << count << ',' << s.backupPrefix.size() << "\n";
        }
    }
}
