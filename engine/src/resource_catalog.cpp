#include <algorithm>

#include "neon/resource_catalog.h"

#include "pugixml.hpp"

#include <cstdlib>
#include <cstring>

namespace neon {

// Element indices follow the original's rule: an explicit index='n' (n > 0)
// sets the slot, otherwise the next slot after the previous one is used.
// (index='0' therefore behaves like "next", which is slot 0 for a first entry.)
static int nextIndex(const pugi::xml_node &n, int &last) {
    int idx = n.attribute("index").as_int(0);
    last = idx > 0 ? idx : last + 1;
    return last;
}

template <typename T> static T &slot(std::vector<T> &v, int i) {
    if (i >= int(v.size()))
        v.resize(size_t(i) + 1);
    return v[size_t(i)];
}

// Parses "page,x,y,w,h" from a Spentry's text. Matches the original scanner:
// each comma-separated field is a sum of signed integer terms, so the data
// may contain small expressions like "123-62" (= 61). '<' (a trailing
// comment) or end of text stops the scan; other characters are ignored.
static SpriteEntry parseSpentry(const char *s) {
    int vals[5] = {0, 0, 0, 0, 0};
    for (int k = 0; k < 5 && *s; k++) {
        int total = 0, term = 0, sign = 1;
        bool digits = false;
        for (; *s && *s != ',' && *s != '<'; s++) {
            if (*s == '-' || *s == '+') {
                if (digits) {
                    total += term * sign;
                    term = 0;
                    digits = false;
                }
                sign = *s == '-' ? -1 : 1;
            } else if (*s >= '0' && *s <= '9') {
                term = term * 10 + (*s - '0');
                digits = true;
            }
        }
        vals[k] = total + term * sign;
        if (*s == ',')
            s++;
        else
            break;
    }
    return {vals[0], vals[1], vals[2], vals[3], vals[4]};
}

bool ResourceCatalog::load(const std::string &xmlText, std::string *error) {
    literals_.clear();
    fonts_.clear();
    spritemaps_.clear();
    pages_.clear();
    samples_.clear();

    pugi::xml_document doc;
    pugi::xml_parse_result r =
        doc.load_buffer(xmlText.data(), xmlText.size(), pugi::parse_default, pugi::encoding_utf8);
    if (!r) {
        if (error)
            *error = r.description();
        return false;
    }
    pugi::xml_node root = doc.child("NeonResources");
    if (!root) {
        if (error)
            *error = "no <NeonResources> element";
        return false;
    }

    int last = -1;
    for (pugi::xml_node n : root.child("Literals").children("Literal"))
        slot(literals_, nextIndex(n, last)) = n.child_value();

    last = -1;
    for (pugi::xml_node n : root.child("Fonts").children("Font")) {
        FontEntry &f = slot(fonts_, nextIndex(n, last));
        f.name = n.child_value();
        f.size = n.attribute("size").as_int(12);
        f.bold = n.attribute("bold").as_int(0) > 0;
        f.italic = n.attribute("italic").as_int(0) > 0;
    }

    int lastMap = -1;
    for (pugi::xml_node m : root.child("Spritemaps").children("Spritemap")) {
        std::vector<SpriteEntry> &map = slot(spritemaps_, nextIndex(m, lastMap));
        int entries = m.attribute("entries").as_int(0);
        if (entries < 1)
            continue;
        map.assign(size_t(entries), SpriteEntry{});
        int lastEntry = -1;
        for (pugi::xml_node e : m.children("Spentry")) {
            int i = nextIndex(e, lastEntry);
            if (i < entries)
                map[size_t(i)] = parseSpentry(e.child_value());
        }
    }

    for (pugi::xml_node n : root.child("SpritePages").children("Spritepage")) {
        SpriteEntry e = parseSpentry(n.child_value()); // "w,h," uses the same scanner
        pages_.push_back({e.page, e.x});
    }

    for (pugi::xml_node n : root.child("Sounds").children("Sample")) {
        SampleEntry s;
        s.file = n.child_value();
        while (!s.file.empty() && (s.file.back() == ' ' || s.file.back() == '\t'))
            s.file.pop_back();
        while (!s.file.empty() && (s.file.front() == ' ' || s.file.front() == '\t'))
            s.file.erase(0, 1);
        s.maxInstances = n.attribute("maxinstance").as_int(1);
        s.priority = std::max(1, n.attribute("priority").as_int(1));
        samples_.push_back(s);
    }

    if (literals_.empty() || spritemaps_.empty()) {
        if (error)
            *error = "NeonResources.xml has no literals or sprite maps";
        return false;
    }
    return true;
}

const SpriteEntry *ResourceCatalog::sprite(int index, int map) const {
    if (map < 0 || map >= int(spritemaps_.size()))
        return nullptr;
    const auto &m = spritemaps_[size_t(map)];
    if (index < 0 || index >= int(m.size()))
        return nullptr;
    return &m[size_t(index)];
}

} // namespace neon
