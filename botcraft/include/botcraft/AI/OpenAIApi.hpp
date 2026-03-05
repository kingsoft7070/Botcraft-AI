#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <botcraft/Network/asio_include.hpp>

namespace Botcraft
{
    class OpenAIApi {
    public:
        OpenAIApi(const std::string& api_key, const std::string& api_url = "https://api.openai.com/v1/chat/completions");
        
        // 新增model参数，支持自定义模型名
        std::string GetAIResponse(const std::string& prompt, const std::string& model = "gpt-3.5-turbo");

    private:
        std::string SendPostRequest(const std::string& post_data);
        std::string ParseResponse(const std::string& json_str);

        std::string api_key;
        std::string api_url;
        asio::io_context io_context;
    };
} // namespace Botcraft
