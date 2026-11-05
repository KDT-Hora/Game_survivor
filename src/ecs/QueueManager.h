#pragma once
#include <functional>
#include <vector>

namespace ECS {

// フレーム末尾で一括実行するコマンドキュー。Entity の生成・破棄はここを経由する。
class QueueManager {
public:
    using Command = std::function<void()>;
    void AddCommand(Command c) { commands.push_back(std::move(c)); }
    // 実行中に追加されたコマンドも同フレームで実行する。
    void Execute() {
        for (size_t i = 0; i < commands.size(); ++i) {
            Command c = std::move(commands[i]);
            c();
        }
        commands.clear();
    }
    bool Empty() const { return commands.empty(); }

private:
    std::vector<Command> commands;
};

}  // namespace ECS
