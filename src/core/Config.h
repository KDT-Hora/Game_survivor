#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace core {

// `key = value` 形式の設定ファイル(# 以降はコメント)。
// バランス数値はここから読む(仕様書 §2, §23-5)。キーが無ければ例外にして、ソース側の既定値に頼らない。
class Config {
public:
    static Config LoadFile(const std::string& path);   // 開けなければ例外
    static Config LoadString(const std::string& text);

    void Set(const std::string& key, const std::string& value) { values[key] = value; }   // 確認用の上書き
    bool Has(const std::string& key) const { return values.count(key) != 0; }
    float F(const std::string& key) const;
    int I(const std::string& key) const;
    bool B(const std::string& key) const { return I(key) != 0; }
    const std::string& S(const std::string& key) const;
    std::vector<std::string> List(const std::string& key) const;  // カンマ区切り
    // prefix で始まるキーを、prefix を取り除いた名前で返す(ゲームモードの上書き用)
    std::vector<std::pair<std::string, std::string>> WithPrefix(const std::string& prefix) const {
        std::vector<std::pair<std::string, std::string>> out;
        for (const auto& kv : values)
            if (kv.first.compare(0, prefix.size(), prefix) == 0) out.push_back({kv.first.substr(prefix.size()), kv.second});
        return out;
    }

private:
    std::map<std::string, std::string> values;
};

}  // namespace core
