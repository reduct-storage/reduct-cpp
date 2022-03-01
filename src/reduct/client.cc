// Copyright 2022 Alexey Timin

#include "reduct/client.h"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "reduct/internal/http_client.h"

namespace reduct {

/**
 * Hidden implement of IClient.
 */
class Client : public IClient {
 public:
  explicit Client(std::string_view url) : url_(url) { client_ = internal::IHttpClient::Build(url); }

  [[nodiscard]] Result<ServerInfo> GetInfo() const noexcept override {
    auto [body, err] = client_->Get("/info");
    if (err) {
      return {{}, std::move(err)};
    }

    try {
      nlohmann::json data;
      data = nlohmann::json::parse(body);
      auto to_ul = [&data](std::string_view key) { return std::stoul(data.at(key.data()).get<std::string>()); };
      return {
          ServerInfo{
              .version = data.at("version"),
              .bucket_count = to_ul("bucket_count"),
              .usage = to_ul("usage"),
              .oldest_record = std::chrono::system_clock::from_time_t(to_ul("oldest_record")),
              .latest_record = std::chrono::system_clock::from_time_t(to_ul("latest_record")),
          },
          Error::kOk,
      };
    } catch (const std::exception& e) {
      return {{}, Error{.code = -1, .message = e.what()}};
    }
  }

  [[nodiscard]] UPtrResult<IBucket> GetBucket(std::string_view name) const noexcept override {
    auto err = client_->Head(fmt::format("/b/{}", name));
    if (err) {
      if (err.code == 404) {
        err.message = fmt::format("Bucket '{}' is not found", name);
      }
      return {{}, std::move(err)};
    }

    return {IBucket::Build(url_, name), {}};
  }

  [[nodiscard]] UPtrResult<IBucket> CreateBucket(std::string_view name,
                                                 IBucket::Settings settings) const noexcept override {
    auto err = client_->Post(fmt::format("/b/{}", name), settings.ToJsonString());
    if (err) {
      return {nullptr, std::move(err)};
    }

    return {IBucket::Build(url_, name), {}};
  }

 private:
  std::unique_ptr<internal::IHttpClient> client_;
  std::string url_;
};

std::unique_ptr<IClient> IClient::Build(std::string_view url) noexcept { return std::make_unique<Client>(url); }

}  // namespace reduct
