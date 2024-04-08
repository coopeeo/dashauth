#pragma once

#include "dashauth.hpp"
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

namespace dashauth {
    class SentDashAuthRequest {
        public:
        SentDashAuthRequest() {
            geode::log::info("what are you doing");
        };
        ~SentDashAuthRequest() {};
        void then(std::function<void(std::string const&)>) {
            this->m_then_callback = callback;

            // then is also responsible for calling the callback
            log::info("requesting auth challenge for {} (base {})", this->m_mod->getID(), this->m_server_url);

            auto token = geode::Mod::get()->getSavedValue<std::string>(fmt::format("dashauth_token_{}", this->m_mod->getID()));

            if (!token.empty()) {
                callback(token);
                return;
            }

            auto account_manager = GJAccountManager::sharedState();

            std::thread request_thread([this, account_manager] {
                geode::utils::web::AsyncWebRequest()
                    .timeout(std::chrono::seconds(6))
                    .fetch(fmt::format("{}/request_challenge/{}", this->m_server_url, account_manager->m_accountID))
                    .json()
                    .then([this, account_manager](matjson::Value const& response) {
                        geode::log::info("nyaaaaaaaa!! {}", response);
                        int account_id = 0;
                        std::string challenge = "";
                        std::string challenge_id = "";

                        // my own servers are non-spec compliant lmao
                        if (response.contains("success") && response["success"].as_bool()) {
                            account_id = response["data"]["bot_account_id"].as_int();
                            challenge = response["data"]["challenge"].as_string();
                            challenge_id = response["data"]["id"].as_string();
                        } else {
                            account_id = response["bot_account_id"].as_int();
                            challenge = response["challenge"].as_string();
                            challenge_id = response["id"].as_string();
                        }

                        // send private message to bot account
                        geode::log::debug("sending auth to bot {} with challenge {}", account_id, challenge);

                        // lambda moment
                        auto self = this;

                        geode::utils::web::AsyncWebRequest()
                            .bodyRaw(fmt::format("{}{}{}{}{}{}{}{}{}{}{}{}{}",
                                "gameVersion=21",
                                "&binaryVersion=35", // idk lmao
                                "&secret=Wmfd2893gb7",
                                "&gjp2=", account_manager->m_GJP2,
                                "&accountID=", std::to_string(account_manager->m_accountID),
                                "&toAccountID=", std::to_string(account_id),
                                "&subject=", cocos2d::ZipUtils::base64URLEncode(fmt::format("auth-{}", challenge)),
                                // body: "meow, delete me if you see this :3 (this is used for authentication)"
                                "&body=XFFdQh0RUFdZVEVRElhUEV1UFUheQRJGVFQURl1YQhQIBhEZQFpcQhFdQRVEQlFWFVdeRhJUREVcV1tFWFdTQVheWhs=",
                                "&secret=Wmfd2893gb7"))
                            .timeout(std::chrono::seconds(6))
                            .userAgent("")
                            .header("Content-Type", "application/x-www-form-urlencoded")
                            .post("https://www.boomlings.com/database/uploadGJMessage20.php")
                            .text()
                            .then([self, challenge_id](std::string const& response) {
                                if (response == "-1") {
                                    auto message = "failed to send gd message";
                                    log::error("{}", message);
                                    self->m_except_callback(message);
                                    return;
                                }
                                geode::log::info("message sent! {}", response);

                                web::AsyncWebRequest()
                                    .timeout(std::chrono::seconds(12))
                                    .fetch(fmt::format("{}/challenge_complete/{}", self->m_server_url, challenge_id)) // TODO: DO NOT HARDCODE THIS !!!
                                    .json()
                                    .then([self](matjson::Value const& response) {
                                        log::info("GOT API ACCESS TOKEN (REAL) (NOT FAKE): {}", response);

                                        std::string token = "";
                                        if (response.contains("success") && response["success"].as_bool()) {
                                            token = response["data"].as_string();
                                        } else {
                                            token = "uh oh";
                                        }

                                        geode::Mod::get()->setSavedValue<std::string>(fmt::format("dashauth_token_{}", self->m_mod->getID()), token);
                                        // this is safe because this runs on the gd thread
                                        self->m_then_callback(token);
                                    })
                                    .expect([self](std::string const& error) {
                                        auto message = fmt::format("failed to get api access token :( {}", error);
                                        log::error("{}", message);
                                        self->m_except_callback(message);
                                        return;
                                    });
                                })
                                .expect([self](std::string const& error) {
                                    auto message = fmt::format("failed to send gd message: {}", error);
                                    log::error("{}", message);
                                    self->m_except_callback(message);
                                    return;
                                });
                    }).expect([this](std::string const& error) {
                        auto message = fmt::format("failed to get challenge: {}", error);
                        log::error("{}", message);
                        this->m_except_callback(message);
                        return;
                    });
            });
            request_thread.detach();
        };

        [[nodiscard]]
        SentDashAuthRequest* except(std::function<void(std::string const&)>) {
            this->m_except_callback = callback;
            return this;
        }

        void initialize(geode::Mod* mod, std::string server_url) {
            this->m_mod = mod;
            this->m_server_url = server_url;
        }
        private:

        std::function<void(std::string const&)> m_then_callback;
        std::function<void(std::string const&)> m_except_callback;
        geode::Mod* m_mod;
        std::string m_server_url;
    };

    class DashAuthRequest {
        public:
        DashAuthRequest() {
            geode::log::info("constructed auth request >~<");
        };
        ~DashAuthRequest() {
        };
        [[nodiscard]]
        SentDashAuthRequest* getToken(geode::Mod* mod, const std::string& server_url) {
            auto request = new SentDashAuthRequest();
            request->initialize(mod, server_url);
            return request;
        }

        friend class SentDashAuthRequest;
    };
};