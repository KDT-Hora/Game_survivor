#include "core/Config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace core {

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

Config Config::LoadString(const std::string& text) {
    Config c;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        line = Trim(line);
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            throw std::runtime_error("config: '=' がありません (行 " + std::to_string(lineNo) + ")");
        c.values[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
    }
    return c;
}

Config Config::LoadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("config: 開けません: " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF) text.erase(0, 3);  // BOM
    return LoadString(text);
}

const std::string& Config::S(const std::string& key) const {
    auto it = values.find(key);
    if (it == values.end()) throw std::runtime_error("config: キーがありません: " + key);
    return it->second;
}
float Config::F(const std::string& key) const { return std::stof(S(key)); }
int Config::I(const std::string& key) const { return std::stoi(S(key)); }

std::vector<std::string> Config::List(const std::string& key) const {
    std::vector<std::string> out;
    std::stringstream ss(S(key));
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = Trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

}  // namespace core
