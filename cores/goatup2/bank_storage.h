#pragma once
#include "stage_bank.h"
#include <filesystem>
#include <utility>
namespace goatup2 {
// Original record tags and bank codec over portable host file services. Records
// overlay bundled banks; they never replace their level geometry or object data.
class BankStorage {
  public:
    explicit BankStorage(std::filesystem::path directory = {}) : directory_(std::move(directory)) {}
    void source(int slot, const std::vector<uint8_t> &encoded, const Bank &);
    void rebase(int slot, const std::vector<uint8_t> &encoded) {
        sources_.at(size_t(slot)) = encoded;
        committed_.at(size_t(slot)).clear();
    }
    bool load(int slot, Bank &, std::string &error);
    bool save(int slot, const Bank &, std::string &error);
    bool loadEdited(int slot, Bank &, std::vector<uint8_t> &encoded, std::string &error);
    bool saveEdited(int slot, const std::vector<uint8_t> &encoded, std::string &error);
    bool saveNamed(const std::string &name, const std::vector<uint8_t> &, std::string &);
    bool loadTouchMode(int &mode, std::string &error, bool *tilt = nullptr);
    bool saveTouchMode(int mode, std::string &error, bool tilt = false);
    static bool encodeRecords(const std::vector<uint8_t> &source, const Bank &records,
                              std::vector<uint8_t> &encoded, std::string &error);

  private:
    std::filesystem::path directory_;
    std::array<std::vector<uint8_t>, 4> sources_;
    std::array<std::vector<int>, 4> committed_;
    int savedTouchMode_ = 0;
    bool savedTilt_ = false;
    std::filesystem::path path(int slot) const;
};
} // namespace goatup2
