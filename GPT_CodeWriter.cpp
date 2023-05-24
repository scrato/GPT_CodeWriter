// GPT_CodeWriter.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <openssl/crypto.h>
#include <cxxopts.hpp>
#include <nlohmann/json.hpp>

#pragma warning(disable:4996)

std::string get_env_var(const std::string& key) {
    const char* val = std::getenv(key.c_str());
    return val == nullptr ? std::string() : std::string(val);
}

int main(int argc, char** argv) {
    cxxopts::Options options("GPT-4 Code Writer",
        "Automatically write or maintain code files using GPT-4");
    options.add_options()
        ("u,user", "User request/message", cxxopts::value<std::string>())
        ("f,file", "Path to code file", cxxopts::value<std::string>())
        ("l,language", "Programming language", cxxopts::value<std::string>())
        ("api", "API key", cxxopts::value<std::string>())
        ("h,help", "Show help");

    cxxopts::ParseResult args;
    try {
        args = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::parsing& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    if (args.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    if (!args.count("user") || !args.count("file") || !args.count("language")) {
        std::cerr << "Error: missing required command line arguments." << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    }

    std::string api_key = args.count("api") ? args["api"].as<std::string>() :
        get_env_var("OPENAI_API_KEY");

    if (api_key.empty()) {
        std::cerr << "Error: missing API key. Provide it via the command line or set the OPENAI_API_KEY environment variable." << std::endl;
        return 1;
    }

    std::string user_msg = args["user"].as<std::string>();
    std::string file_path = args["file"].as<std::string>();
    std::string language = args["language"].as<std::string>();

    httplib::Client client("https://api.openai.com");
    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key},
        {"Content-Type", "application/json"},
    };
    nlohmann::json req_body = {
        {"model", "gpt-4"}        
    };

    std::ifstream file_input(file_path);
    if (file_input) {
        std::string file_contents((std::istreambuf_iterator<char>(file_input)),
            std::istreambuf_iterator<char>());
        req_body["messages"].push_back({ {"role", "system"},
                                        {"content", "Fix or change the following code in the programming language '" + language + "': " + file_contents + "---- Output JUST the new content, nothing else"}});
    }
    else {
        req_body["messages"].push_back({ {"role", "system"},
                                        {"content", "Output JUST the source code (and nothing else) for the described file, using the programming language '" + language + "'"} });
    }
    req_body["messages"].push_back({{"role", "user"}, {"content", user_msg}});

    std::string req_body_str = req_body.dump();

    client.set_connection_timeout(time_t(60000));
    client.set_read_timeout(time_t(60000));
    client.set_write_timeout(time_t(60000));
    httplib::Result response = client.Post("/v1/chat/completions", headers, req_body_str, "application/json");

    if (response && response->status == 200) {
        auto json_response = nlohmann::json::parse(response->body);
        std::string code = json_response.at("choices").at(0).at("message").at("content").get<std::string>();

        std::ofstream file_output(file_path);
        if (file_output) {
            file_output.write(code.c_str(), code.size());
            file_output.close();
            std::cout << "File successfully created or updated at: " << file_path << std::endl;
        }
        else {
            std::cerr << "Error: unable to write to file." << std::endl;
            return 1;
        }
    }
    else {
        std::cerr << "Error: GPT-4 API request failed." << std::endl;
        return 1;
    }

    return 0;
}