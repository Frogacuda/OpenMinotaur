#pragma once
#include <istream>
#include <ostream>
namespace minotron::module {
// Remember committed games, rather than menu previews. Explicit selection
// takes precedence over later controller connection/disconnection.
class ModePreference {
    int played_ = -1;
    bool selected_ = false;

  public:
    void read(std::istream &in) {
        played_ = -1;
        selected_ = false;
        int value = -1;
        if (in >> value && value >= 0 && value < 4)
            played_ = value;
    }
    int initial(bool controller) const {
        return played_ >= 0 ? played_ : controller ? 0 : 2;
    }
    bool automatic() const {
        return played_ < 0 && !selected_;
    }
    void selected() {
        selected_ = true;
    }
    bool played(int mode) {
        if (mode < 0 || mode >= 4)
            return false;
        selected_ = true;
        if (played_ == mode)
            return false;
        played_ = mode;
        return true;
    }
    bool available() const {
        return played_ >= 0;
    }
    void write(std::ostream &out) const {
        if (available())
            out << played_ << '\n';
    }
};
} // namespace minotron::module
