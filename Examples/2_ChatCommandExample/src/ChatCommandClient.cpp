#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include "getopt.h" // 需提前添加Windows版getopt.h（见下方说明）
#include "botcraft/AI/OpenAIApi.hpp"
#include "botcraft/AI/BehaviourClient.hpp"
#include "botcraft/AI/Blackboard.hpp"
#include "botcraft/AI/Tasks/BaseTasks.hpp"
#include "botcraft/AI/Tasks/DigTask.hpp"
#include "botcraft/AI/Tasks/PathfindingTask.hpp"
#include "botcraft/Game/Entities/LocalPlayer.hpp"
#include "botcraft/Game/World/World.hpp"
#include "botcraft/Game/Entities/Entity.hpp"
#include "botcraft/Game/Enums/EntityType.hpp"
#include "botcraft/Game/Enums/BlockType.hpp"
#include "botcraft/Utilities/Logger.hpp"
#include "botcraft/Network/Connection.hpp"

// 全局配置结构体（包含所有可配置参数）
struct BotConfig {
    std::string server_ip = "localhost";                // MC服务器IP
    int server_port = 25565;                            // MC服务器端口
    std::string your_mc_name = "Player";                // 你的MC名字（指令过滤）
    std::string openai_api_key = "";                    // AI API密钥
    std::string bot_name = "AI_ToolMan";                // 机器人游戏名
    std::string base_url = "https://api.openai.com/v1/chat/completions"; // AI接口地址
    std::string model_name = "gpt-3.5-turbo";           // AI模型名
} g_config;

// 全局AI API实例
std::unique_ptr<Botcraft::OpenAIApi> g_ai_api;

// 跟随玩家Task实现
Botcraft::BehaviourStatus FollowPlayerTask(Botcraft::BehaviourClient& client, Botcraft::Blackboard& blackboard)
{
    const std::string target_player_name = blackboard.Get<std::string>("follow_target", g_config.your_mc_name);
    
    auto world = client.GetWorld();
    auto local_player = client.GetLocalPlayer();
    if (!world || !local_player)
    {
        Botcraft::Logger::GetInstance().LogWarning("无法获取世界/本地玩家");
        return Botcraft::BehaviourStatus::Failure;
    }

    std::shared_ptr<Botcraft::Entity> target_player = nullptr;
    for (const auto& [id, entity] : world->GetEntities())
    {
        if (entity->GetType() == Botcraft::EntityType::Player && entity->GetName() == target_player_name)
        {
            target_player = entity;
            break;
        }
    }

    if (!target_player)
    {
        Botcraft::Logger::GetInstance().LogWarning("未找到玩家: " + target_player_name);
        return Botcraft::BehaviourStatus::Failure;
    }

    // 跟随位置：玩家身后1格，避免卡位
    const Botcraft::Position target_pos = target_player->GetPosition() - (target_player->GetLookVector() * 1.0f) + Botcraft::Position(0, 0, 0);
    const double distance = local_player->GetPosition().Distance(target_pos);

    if (distance > 2.0)
    {
        blackboard.Set("pathfinding_target", target_pos);
        return Botcraft::PathfindingTask()(client, blackboard);
    }
    else
    {
        local_player->SetVelocity(Botcraft::Vector3<double>(0, 0, 0));
        return Botcraft::BehaviourStatus::Success;
    }
}

// 执行AI解析后的指令
void ExecuteAIAction(const std::string& action, Botcraft::BehaviourClient& client) {
    auto& blackboard = client.GetBlackboard();
    auto player = client.GetLocalPlayer();
    if (!player) return;

    // 设置跟随目标为你的MC名字
    blackboard.Set("follow_target", g_config.your_mc_name);

    Botcraft::Logger::GetInstance().LogInfo("执行AI指令: " + action);

    if (action == "follow_me") {
        // 持续跟随，直到收到新指令
        while (true) {
            FollowPlayerTask(client, blackboard);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    } else if (action == "mine_diamond") {
        // 自动挖钻石，找不到则返回跟随
        blackboard.Set("dig_target_block", Botcraft::BlockType::DiamondOre);
        while (true) {
            const auto diamond_pos = client.GetWorld()->FindNearestBlock(
                Botcraft::BlockType::DiamondOre, 
                player->GetPosition(),
                200
            );
            if (diamond_pos == Botcraft::Position::ZERO) {
                client.GetConnection()->SendChatMessage("没找到钻石矿，回到你身边~");
                break;
            }
            blackboard.Set("pathfinding_target", diamond_pos);
            Botcraft::PathfindingTask()(client, blackboard);
            Botcraft::DigTask()(client, blackboard);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        ExecuteAIAction("follow_me", client);
    } else if (action == "mine_wood") {
        // 自动砍木头
        blackboard.Set("dig_target_block", Botcraft::BlockType::OakLog);
        while (true) {
            const auto wood_pos = client.GetWorld()->FindNearestBlock(
                Botcraft::BlockType::OakLog, 
                player->GetPosition(),
                200
            );
            if (wood_pos == Botcraft::Position::ZERO) {
                client.GetConnection()->SendChatMessage("没找到木头，回到你身边~");
                break;
            }
            blackboard.Set("pathfinding_target", wood_pos);
            Botcraft::PathfindingTask()(client, blackboard);
            Botcraft::DigTask()(client, blackboard);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        ExecuteAIAction("follow_me", client);
    } else if (action == "escape_monster") {
        // 躲避怪物
        blackboard.Set("avoid_entity_types", std::vector<Botcraft::EntityType>{
            Botcraft::EntityType::Zombie,
            Botcraft::EntityType::Creeper,
            Botcraft::EntityType::Skeleton
        });
        Botcraft::AvoidEntitiesTask()(client, blackboard);
        ExecuteAIAction("follow_me", client);
    }
}

// 聊天指令处理类
class ChatCommandClient : public Botcraft::BehaviourClient
{
public:
    using Botcraft::BehaviourClient::BehaviourClient;

    void HandleChatMessage(const std::string& msg) override
    {
        // 只响应包含你MC名字的指令
        if (msg.find(g_config.your_mc_name) == std::string::npos) return;

        // 解析"ai "开头的指令
        const size_t ai_prefix = msg.find("ai ");
        if (ai_prefix != std::string::npos) {
            const std::string ai_prompt = msg.substr(ai_prefix + 3);
            // 调用AI API解析指令（传入自定义模型名）
            const std::string ai_action = g_ai_api->GetAIResponse(ai_prompt, g_config.model_name);
            if (!ai_action.empty()) {
                // 新开线程执行，避免阻塞
                std::thread t(ExecuteAIAction, ai_action, std::ref(*this));
                t.detach();
            }
        }
    }

    void SetBotName(const std::string& name) { this->bot_name = name; }
private:
    std::string bot_name;
};

// 主函数：解析命令行参数 + 启动机器人
int main(int argc, char* argv[]) {
    // 1. 解析命令行参数
    const char* short_options = "h:i:p:n:k:b:u:m:";
    const struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"ip", required_argument, nullptr, 'i'},
        {"port", required_argument, nullptr, 'p'},
        {"your-name", required_argument, nullptr, 'n'},
        {"api-key", required_argument, nullptr, 'k'},
        {"bot-name", required_argument, nullptr, 'b'},
        {"base-url", required_argument, nullptr, 'u'},
        {"model", required_argument, nullptr, 'm'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'h': // 帮助信息
                std::cout << "Minecraft AI工具人机器人 - 使用说明\n"
                          << "用法：" << argv[0] << " [参数]\n\n"
                          << "必选参数：\n"
                          << "  -n/--your-name <名字>  你的MC游戏名字（指令过滤，必填）\n"
                          << "  -k/--api-key <密钥>    AI API密钥（必填）\n\n"
                          << "可选参数：\n"
                          << "  -h/--help              显示帮助信息\n"
                          << "  -i/--ip <IP>           MC服务器IP（默认：localhost）\n"
                          << "  -p/--port <端口>       MC服务器端口（默认：25565）\n"
                          << "  -b/--bot-name <名字>   机器人在游戏中的名字（默认：AI_ToolMan）\n"
                          << "  -u/--base-url <URL>    AI接口地址（默认：OpenAI官方）\n"
                          << "  -m/--model <模型名>    AI模型名称（默认：gpt-3.5-turbo）\n\n"
                          << "示例：\n"
                          << argv[0] << " --your-name \"我的MC名\" --api-key \"sk-xxx\" --ip 192.168.1.100 --model qwen-turbo\n";
                return 0;
            case 'i':
                g_config.server_ip = optarg;
                break;
            case 'p':
                g_config.server_port = std::stoi(optarg);
                break;
            case 'n':
                g_config.your_mc_name = optarg;
                break;
            case 'k':
                g_config.openai_api_key = optarg;
                break;
            case 'b':
                g_config.bot_name = optarg;
                break;
            case 'u':
                g_config.base_url = optarg;
                break;
            case 'm':
                g_config.model_name = optarg;
                break;
            default:
                std::cerr << "参数错误！使用 -h 查看帮助。" << std::endl;
                return 1;
        }
    }

    // 校验必填参数
    if (g_config.your_mc_name.empty() || g_config.openai_api_key.empty()) {
        std::cerr << "\033[31m错误：必须指定 --your-name 和 --api-key！\033[0m" << std::endl;
        std::cerr << "示例：" << argv[0] << " --your-name \"张三\" --api-key \"sk-xxxxxx\"" << std::endl;
        return 1;
    }

    // 2. 初始化AI API（传入密钥+base_url）
    g_ai_api = std::make_unique<Botcraft::OpenAIApi>(g_config.openai_api_key, g_config.base_url);

    // 3. 初始化机器人客户端
    ChatCommandClient client;
    client.SetBotName(g_config.bot_name);
    
    // 连接MC服务器
    std::cout << "正在连接服务器：" << g_config.server_ip << ":" << g_config.server_port << std::endl;
    if (!client.Connect(g_config.server_ip, g_config.server_port)) {
        std::cerr << "\033[31m连接服务器失败！请检查IP/端口是否正确。\033[0m" << std::endl;
        return 1;
    }

    // 4. 启动机器人
    std::cout << "\033[32m机器人启动成功！\033[0m" << std::endl;
    std::cout << "你的MC名字：" << g_config.your_mc_name << std::endl;
    std::cout << "AI模型：" << g_config.model_name << std::endl;
    std::cout << "游戏内发送 'ai 指令' 控制（示例：ai 跟我走、ai 挖钻石、ai 砍木头）" << std::endl;
    client.Run();

    return 0;
}

// Windows版getopt.h实现（需单独保存为getopt.h）
#ifndef GETOPT_H
#define GETOPT_H
extern char* optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char* const argv[], const char* optstring);
struct option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};
int getopt_long(int argc, char* const argv[], const char* optstring,
                const struct option* longopts, int* longindex);
#define no_argument 0
#define required_argument 1
#define optional_argument 2
#endif // GETOPT_H
