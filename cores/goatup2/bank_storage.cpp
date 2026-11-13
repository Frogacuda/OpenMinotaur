#include "bank_storage.h"
#include <fstream>
#include <sstream>
#include <iterator>
#include <stdexcept>
namespace goatup2 {
namespace {
std::vector<int> records(const Bank &b) {
    std::vector<int> values{b.pur, b.tim, b.fur};
    for (const auto &s : b.stages) {
        values.push_back(s.highScore);
        values.push_back(s.bestTime);
    }
    return values;
}
bool identity(const Bank &a, const Bank &b) {
    if (a.gid != b.gid)
        return false;
    for (size_t i = 0; i < a.stages.size(); ++i) {
        const auto &x = a.stages[i];
        const auto &y = b.stages[i];
        if (x.seed != y.seed || x.cells != y.cells || x.backupPrefix != y.backupPrefix ||
            x.declaredColumns != y.declaredColumns || x.declaredRows != y.declaredRows ||
            x.displayWidth != y.displayWidth || x.displayHeight != y.displayHeight ||
            x.objects.size() != y.objects.size())
            return false;
        for (size_t j = 0; j < x.objects.size(); ++j)
            if (x.objects[j].type != y.objects[j].type ||
                x.objects[j].variant != y.objects[j].variant || x.objects[j].x != y.objects[j].x ||
                x.objects[j].y != y.objects[j].y)
                return false;
    }
    return true;
}
void overlay(const Bank &from, Bank &to) {
    to.pur = from.pur;
    to.tim = from.tim;
    to.fur = from.fur;
    for (size_t i = 0; i < to.stages.size(); ++i) {
        to.stages[i].highScore = from.stages[i].highScore;
        to.stages[i].bestTime = from.stages[i].bestTime;
    }
}
bool read(const std::filesystem::path &path, std::vector<uint8_t> &bytes, std::string &error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "cannot open " + path.u8string();
        return false;
    }
    const auto length = file.tellg();
    if (length <= 0 || length > std::streamoff(BankCapacity)) {
        error = "invalid save length";
        return false;
    }
    bytes.resize(size_t(length));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(bytes.data()), length);
    if (!file) {
        error = "incomplete save read";
        return false;
    }
    return true;
}
// Same recoverable temp/backup replacement used by Gridrunner's checkpoint
// service. Paths stay inside the game's host save directory; no asset writes.
bool write(const std::filesystem::path &path, const std::vector<uint8_t> &bytes,
           std::string &error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "cannot create save directory: " + ec.message();
        return false;
    }
    auto temp = path;
    temp += ".tmp";
    auto backup = path;
    backup += ".bak";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = "cannot open save temporary file";
            return false;
        }
        file.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
        file.close();
        if (!file) {
            error = "cannot finish save temporary file";
            return false;
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (!ec)
        return true;
    ec.clear();
    if (!std::filesystem::exists(path, ec) || ec) {
        error = "cannot commit save";
        return false;
    }
    std::filesystem::remove(backup, ec);
    if (ec) {
        error = "cannot rotate save backup";
        return false;
    }
    std::filesystem::rename(path, backup, ec);
    if (ec) {
        error = "cannot retain previous save";
        return false;
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::error_code rollback;
        std::filesystem::rename(backup, path, rollback);
        error = "cannot commit replacement save";
        return false;
    }
    // Keep one last-good copy, also usable after an interrupted Windows replace.
    return true;
}
void tag(std::string &section, const char *name, int value) {
    const auto at = section.find(name);
    if (at == std::string::npos)
        throw std::runtime_error("record tag missing");
    const auto start = at + std::char_traits<char>::length(name);
    const auto end = section.find_first_of("\r\n(", start);
    section.replace(start, end == std::string::npos ? end : end - start, std::to_string(value));
}
bool loadEncoded(const std::filesystem::path &path, const Bank &expected, Bank &loaded,
                 std::string &error) {
    std::vector<uint8_t> bytes;
    if (!read(path, bytes, error) || !readBank(bytes, loaded, error))
        return false;
    if (!identity(expected, loaded)) {
        error = "save belongs to different bank/level seeds";
        return false;
    }
    if (loaded.pur < 0 || loaded.tim < 0 || loaded.fur < 0 || loaded.fur >= 20) {
        error = "invalid bank records";
        return false;
    }
    for (const auto &s : loaded.stages)
        if (s.highScore < 0 || s.bestTime < 0) {
            error = "invalid level records";
            return false;
        }
    return true;
}
} // namespace
std::filesystem::path BankStorage::path(int slot) const {
    return directory_ / ("goatup2-bank-" + std::to_string(slot) + ".dat");
}
void BankStorage::source(int slot, const std::vector<uint8_t> &encoded, const Bank &b) {
    sources_.at(size_t(slot)) = encoded;
    committed_.at(size_t(slot)) = records(b);
}
bool BankStorage::encodeRecords(const std::vector<uint8_t> &source, const Bank &b,
                                std::vector<uint8_t> &encoded, std::string &error) {
    try {
        Bank original;
        if (!readBank(source, original, error))
            return false;
        if (!identity(original, b)) {
            error = "record/source identity mismatch";
            return false;
        }
        const auto raw = transformBank(source);
        std::string text(raw.begin(), raw.end());
        for (int i = 0; i < 20; ++i) {
            const auto label = "(L" + std::to_string(i) + ")";
            const auto at = text.find(label), end = text.find("(XEND)", at);
            if (at == std::string::npos || end == std::string::npos)
                throw std::runtime_error("level section missing");
            if (!b.stages[size_t(i)].seed)
                continue;
            auto section = text.substr(at, end + 6 - at);
            tag(section, "(LHS)", b.stages[size_t(i)].highScore);
            tag(section, "(LBT)", b.stages[size_t(i)].bestTime);
            text.replace(at, end + 6 - at, section);
        }
        const auto at = text.find("(L123)"), end = text.find("(XEND)", at);
        if (at == std::string::npos || end == std::string::npos)
            throw std::runtime_error("bank header missing");
        auto section = text.substr(at, end + 6 - at);
        tag(section, "(PUR)", b.pur);
        tag(section, "(TIM)", b.tim);
        tag(section, "(FUR)", b.fur);
        text.replace(at, end + 6 - at, section);
        if (text.size() > BankCapacity) {
            error = "record update exceeds original bank capacity";
            return false;
        }
        // Preserve every geometry/coordinate string and trailing byte. No RNG
        // touches the live game: the bank codec has its own reset stream.
        std::vector<uint8_t> bytes(text.begin(), text.end());
        auto result = transformBank(bytes);
        Bank roundtrip;
        if (!readBank(result, roundtrip, error) || records(roundtrip) != records(b))
            return false;
        encoded = std::move(result);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool BankStorage::load(int slot, Bank &b, std::string &error) {
    auto file = path(slot), backup = file;
    backup += ".bak";
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        if (ec) {
            error = ec.message();
            return false;
        }
        if (!std::filesystem::exists(backup, ec)) {
            error = ec ? ec.message() : "";
            return !ec;
        }
        file = backup;
    }
    Bank loaded;
    if (!loadEncoded(file, b, loaded, error))
        return false;
    overlay(loaded, b);
    committed_.at(size_t(slot)) = records(b);
    error.clear();
    return true;
}
bool BankStorage::save(int slot, const Bank &b, std::string &error) {
    const auto values = records(b);
    if (values == committed_.at(size_t(slot))) {
        error.clear();
        return true;
    }
    std::vector<uint8_t> bytes;
    if (!encodeRecords(sources_.at(size_t(slot)), b, bytes, error) ||
        !write(path(slot), bytes, error))
        return false;
    committed_[size_t(slot)] = values;
    error.clear();
    return true;
}
bool BankStorage::loadEdited(int slot, Bank &bank, std::vector<uint8_t> &encoded,
                             std::string &error) {
    if (slot < 0 || slot > 3) {
        error = "invalid bank slot";
        return false;
    }
    auto file = directory_ / ("goatup2-editor-bank-" + std::to_string(slot) + ".dat");
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        auto backup = file;
        backup += ".bak";
        if (!std::filesystem::exists(backup, ec)) {
            error = ec ? ec.message() : "";
            return !ec;
        }
        file = backup;
    }
    std::vector<uint8_t> data;
    Bank loaded;
    if (!read(file, data, error) || !readBank(data, loaded, error) ||
        !validateRuntimeBank(loaded, error))
        return false;
    bank = std::move(loaded);
    encoded = std::move(data);
    error.clear();
    return true;
}
bool BankStorage::saveEdited(int slot, const std::vector<uint8_t> &encoded, std::string &error) {
    if (slot < 0 || slot > 3) {
        error = "invalid bank slot";
        return false;
    }
    Bank checked;
    if (!readBank(encoded, checked, error) || !validateRuntimeBank(checked, error))
        return false;
    return write(directory_ / ("goatup2-editor-bank-" + std::to_string(slot) + ".dat"), encoded,
                 error);
}
bool BankStorage::saveNamed(const std::string &name, const std::vector<uint8_t> &encoded,
                            std::string &error) {
    std::string filename = name;
    for (auto &c : filename)
        if (uint8_t(c) < 32 || std::string("<>:\"/\\|?*").find(c) != std::string::npos)
            c = '_';
    while (!filename.empty() && (filename.back() == '.' || filename.back() == ' '))
        filename.pop_back();
    if (filename.empty() || filename == "." || filename == "..") {
        error = "invalid bank filename";
        return false;
    }
    Bank check;
    if (!readBank(encoded, check, error) || !validateRuntimeBank(check, error))
        return false;
    return write(directory_ / (filename + ".gu2"), encoded, error);
}
bool BankStorage::loadTouchMode(int &mode, std::string &error, bool *tilt) {
    const auto file = directory_ / "goatup2-controls.txt";
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        error = ec ? ec.message() : "";
        return !ec;
    }
    std::ifstream input(file);
    std::string magic, extra;
    int version = 0, value = -1, tiltValue = 0;
    if (!(input >> magic >> version >> value) || magic != "GU2-CONTROLS" ||
        (version != 1 && version != 2) || value < 0 || value > 1 ||
        (version == 2 && (!(input >> tiltValue) || tiltValue < 0 || tiltValue > 1)) ||
        (input >> extra)) {
        error = "invalid GoatUp 2 control preferences";
        return false;
    }
    savedTilt_ = tiltValue != 0;
    if (tilt)
        *tilt = savedTilt_;
    mode = savedTouchMode_ = value;
    error.clear();
    return true;
}
bool BankStorage::saveTouchMode(int mode, std::string &error, bool tilt) {
    if (mode < 0 || mode > 1) {
        error = "invalid touch mode";
        return false;
    }
    if (mode == savedTouchMode_ && tilt == savedTilt_) {
        error.clear();
        return true;
    }
    const auto text =
        "GU2-CONTROLS 2\n" + std::to_string(mode) + " " + std::to_string(int(tilt)) + "\n";
    if (!write(directory_ / "goatup2-controls.txt", {text.begin(), text.end()}, error))
        return false;
    savedTouchMode_ = mode;
    savedTilt_ = tilt;
    return true;
}
} // namespace goatup2
