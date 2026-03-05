#include "botcraft/AI/OpenAIApi.hpp"
#include "botcraft/Network/HttpClient.hpp"
#include "botcraft/Utilities/Logger.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

namespace Botcraft
{
    OpenAIApi::OpenAIApi(const std::string& api_key, const std::string& api_url)
        : api_key(api_key), api_url(api_url) {}

    std::string OpenAIApi::GetAIResponse(const std::string& prompt, const std::string& model) {
        // 构造AI请求体（使用自定义模型名）
        json request_body = {
            {"model", model},
            {"messages", {
                {{"role", "system"}, {"content", "你是Minecraft机器人的指令解析器，仅返回单个行为关键词：follow_me/mine_diamond/mine_wood/escape_monster，无多余文字。"}},
                {{"role", "user"}, {"content", prompt}}
            }},
            {"temperature", 0.1}
        };

        const std::string post_data = request_body.dump();
        const std::string response = SendPostRequest(post_data);

        return ParseResponse(response);
    }

    std::string OpenAIApi::SendPostRequest(const std::string& post_data) {
        try {
            asio::ssl::context ssl_context(asio::ssl::context::tls_client);
            HttpClient client(io_context, ssl_context);

            std::vector<std::pair<std::string, std::string>> headers = {
                {"Authorization", "Bearer " + api_key},
                {"Content-Type", "application/json"}
            };

            std::string response;
            client.Post(api_url, headers, post_data, response);
            return response;
        } catch (const std::exception& e) {
            Logger::GetInstance().LogError("AI API请求失败: " + std::string(e.what()));
            return "";
        }
    }

    std::string OpenAIApi::ParseResponse(const std::string& json_str) {
        try {
            if (json_str.empty()) {
                Logger::GetInstance().LogWarning("AI响应为空");
                return "";
            }

            const json response = json::parse(json_str);
            std::string content = response["choices"][0]["message"]["content"];
            // 清洗空格/换行
            content.erase(std::remove_if(content.begin(), content.end(), isspace), content.end());
            return content;
        } catch (const std::exception& e) {
            Logger::GetInstance().LogError("AI响应解析失败: " + std::string(e.what()));
            return "";
        }
    }
} // namespace Botcraft
