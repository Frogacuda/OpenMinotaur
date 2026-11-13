#include "title_sequence.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace neon::widgets;
namespace fs = std::filesystem;
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string read(const fs::path &path) {
    std::ifstream file(path);
    check(bool(file), "Cannot read title definition");
    return {std::istreambuf_iterator<char>(file), {}};
}
const Draw &find(const Frame &frame, const std::string &name) {
    for (const auto &draw : frame.draws)
        if (draw.path == name)
            return draw;
    throw std::runtime_error("Missing title draw: " + name);
}
void advance(TitleSequence &title, double seconds, Viewport viewport = {}) {
    while (seconds > 1e-9) {
        double step = std::min(seconds, .01);
        title.update(step, viewport);
        seconds -= step;
    }
}
int main(int argc, char **argv) {
    try {
        check(argc == 2, "Usage: mino_title_sequence_checks assets/title");
        fs::path folder = argv[1];
        auto document = Value::parse(read(folder / "Minotaur-Title.widgetml"));
        TitleSequence title(document);
        check(title.maxFrameRate() == 60, "Title frame-rate cap changed");
        for (auto size : {std::array<int, 2>{1600, 900}, {900, 1600}, {1200, 900}, {900, 1200}}) {
            const auto view = title.presentationViewport(size[0], size[1]);
            check(view.x == 0 && view.y == 0 && view.width == size[0] && view.height == size[1],
                  "Supported title aspect was letterboxed");
        }
        const auto wide = title.presentationViewport(2000, 900);
        const auto tall = title.presentationViewport(900, 2000);
        check(wide.x == 200 && wide.y == 0 && wide.width == 1600 && wide.height == 900,
              "Wide title viewport was not centered at 16:9");
        check(tall.x == 0 && tall.y == 200 && tall.width == 900 && tall.height == 1600,
              "Tall title viewport was not centered at 9:16");
        title.update(0, {});
        check(title.opacity() == 0, "Title did not start at black");
        advance(title, .01);
        double initialScale = find(title.frame(), "title/logo/minotaur-face").transform.a;
        advance(title, .39);
        check(title.opacity() > .45 && title.opacity() < .55, "Intro fade midpoint changed");
        advance(title, .41);
        double fullScale = find(title.frame(), "title/logo/minotaur-face").transform.a;
        double t = .01 / .8;
        double expectedScale = .75 + .25 * t * t * (3 - 2 * t);
        check(std::abs(initialScale / fullScale - expectedScale) < 1e-6,
              "Logo no longer zooms from 75 percent");
        check(title.opacity() == 1, "Intro did not reach full opacity");
        advance(title, 1);
        check(!title.takeLaunchRequest(), "Title launched without input");
        title.input(true, false);
        title.update(0, {});
        advance(title, .25);
        check(title.opacity() > .45 && title.opacity() < .55, "Exit fade midpoint changed");
        check(!title.takeLaunchRequest(), "Handoff preceded fade completion");
        advance(title, .26);
        check(title.finished() && title.opacity() == 0 && title.takeLaunchRequest(),
              "Exit did not hand off at black");
        check(!title.takeLaunchRequest(), "Handoff emitted twice");

        title.begin(true, true);
        advance(title, 2);
        title.input(true, false);
        advance(title, .6);
        check(!title.finished(), "Held launch input dismissed the title");
        title.input(false, false);
        title.input(false, true);
        title.update(0, {});
        advance(title, .51);
        check(title.takeLaunchRequest(), "Fresh pointer press failed to dismiss");

        title.begin();
        advance(title, .2);
        double beforePress = title.opacity();
        title.input(true, false);
        title.update(0, {});
        check(std::abs(title.opacity() - beforePress) < 1e-8,
              "Early dismissal caused a brightness jump");
        advance(title, .51);
        check(title.takeLaunchRequest(), "Intro-time input was lost");

        for (auto size : {std::array<int, 2>{1600, 900}, {900, 1600}}) {
            Viewport viewport;
            viewport.width = size[0];
            viewport.height = size[1];
            title.begin();
            advance(title, 2, viewport);
            const auto &legal = find(title.frame(), "title/legal/wording");
            auto bottom = legal.transform.point(0, legal.bounds.height);
            double scale = std::min(size[0], size[1]) / 1000.;
            check(std::abs(bottom[1] - (size[1] - 18 * scale)) < 1e-6,
                  "Footer lost bottom-edge placement");
        }

        std::set<std::string> expected{"Minotaur-Title.widgetml"}, actual;
        for (const auto &asset : document.at("assets").object())
            expected.insert(asset.second.string());
        for (const auto &item : fs::recursive_directory_iterator(folder))
            if (item.is_regular_file())
                actual.insert(fs::relative(item.path(), folder).generic_string());
        check(actual == expected && actual.size() == 6,
              "Title bundle contains missing or unused assets");
        std::cout << "PASS: automatic intro, logo zoom, held-input gate, button/pointer exit, "
                     "early dismissal, fade-to-black handoff, single delivery, restart, "
                     "orientation and exact asset closure.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
