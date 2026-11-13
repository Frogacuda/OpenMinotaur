#include "achievements.h"
#include "core_api.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
struct TestHost : mino::Host {
    std::set<std::string> reports;
    bool readAsset(const std::string &, std::vector<uint8_t> &) const override {
        return false;
    }
    std::string saveDir() const override {
        return {};
    }
    void log(const char *, ...) override {}
    std::string adaptShader(const std::string &, int) const override {
        return {};
    }
    void lockAudio() override {}
    void unlockAudio() override {}
    void requestQuit() override {}
    void unlockAchievement(const std::string &id) override {
        reports.insert(id);
    }
};
int main(int argc, char **argv) {
    try {
        TestHost host;
        host.observeOriginalAchievements(0x80000005u);
        require(host.reports == std::set<std::string>{"original-0", "original-2", "original-31"},
                "Simultaneous source awards survive mailbox overwrites; high bit remains unsigned");
        require(argc == 2, "Supply a disposable scratch directory");
        const auto dir = std::filesystem::u8path(argv[1]);
        std::filesystem::create_directories(dir);
        const auto path = (dir / "achievements.txt").string();
        std::filesystem::remove(path);
        mino::Achievements a;
        require(a.load(path) && !a.dirty(), "First run");
        require(a.unlock("minotron", "original-0"), "First award");
        require(!a.unlock("minotron", "original-0"), "Repeat award is idempotent");
        require(a.unlock("minorescue", "original-0"), "Game namespaces remain separate");
        require(a.unlock("future-game", "custom-goal"),
                "Unknown catalog entries remain representable");
        require(!a.unlock("../bad", "goal") && !a.unlock("minotron", "bad key"),
                "Reject malformed identities");
        require(!a.save((dir / "missing" / "records").string()) && a.dirty(),
                "Failed write retains pending awards");
        require(a.save(path) && !a.dirty(), "Save retry");
        mino::Achievements b;
        require(b.load(path) && b.entries() == a.entries(),
                "Restart retains awards and unknown keys");
        require(b.unlock("minos", "original-9") && b.save(path), "Replace existing file");
        require(a.load(path) && a.contains("minos", "original-9"), "Replacement visible");
        // A directory at the temporary filename forces failure before replacement.
        std::filesystem::create_directory(path + ".tmp");
        require(a.unlock("deflex", "original-2") && !a.save(path) && a.dirty(),
                "Blocked temporary write");
        require(b.load(path) && !b.contains("deflex", "original-2"),
                "Previous file survives failure");
        std::filesystem::remove(path + ".tmp");
        require(a.save(path), "Retry after obstruction");
        {
            std::ofstream out(path, std::ios::binary);
            out << "minotaur-achievements 1\r\nminotron original-0\r\n";
        }
        require(b.load(path) && b.contains("minotron", "original-0"), "Portable CRLF save format");
        for (const auto &content : {"minotaur-achievements 2\nminotron original-0\n",
                                    "minotaur-achievements 1\nminotron original-0\nbroken\n"}) {
            {
                std::ofstream out(path);
                out << content;
            }
            require(!b.load(path), "Reject unsupported or malformed file as a whole");
            b.unlock("minotron", "original-2");
            require(!b.save(path), "Protect unreadable file from overwrite");
            std::ifstream in(path);
            const std::string retained((std::istreambuf_iterator<char>(in)), {});
            require(retained == content, "Protected file unchanged");
        }
        std::cout
            << "Achievement persistence, identities, repeat awards, migration union and failure protection passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
