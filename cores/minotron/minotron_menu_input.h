#pragma once
namespace minotron {
namespace module {
// A controller activation must consume its confirm hold so the new info
// page is not immediately dismissed. No touch identity is involved.
class InfoPageButtonGate {
  public:
    void menuActivated(int label) {
        if (label == 4 || label == 18 || label == 20)
            waitingForRelease_ = true;
    }
    bool filter(bool held) {
        if (!held)
            waitingForRelease_ = false;
        return !waitingForRelease_;
    }

  private:
    bool waitingForRelease_ = false;
};
} // namespace module
} // namespace minotron
