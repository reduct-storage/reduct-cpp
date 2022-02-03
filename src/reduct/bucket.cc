// Copyright 2022 Alexey Timin

#include "reduct/bucket.h"

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "reduct/internal/http_client.h"

namespace reduct {

class Bucket : public IBucket {
 public:
  Bucket(std::string_view url, std::string_view name) : path_(fmt::format("/b/{}", name)) {
    client_ = internal::IHttpClient::Build(url);
  }

  Result<Settings> GetSettings() const noexcept override {
    auto [body, err] = client_->Get(path_);
    if (err) {
      return {{}, std::move(err)};
    }
    return Settings::Parse(body);
  }

  Error UpdateSettings(const Settings& settings) const noexcept override {
    auto [current_setting, get_err] = GetSettings();
    if (get_err) {
      return get_err;
    }

    // TODO(Alexey Timin): Make PUT request parameters optional to avoid this
    if (settings.max_block_size) current_setting.max_block_size = settings.max_block_size;
    if (settings.quota_type) current_setting.quota_type = settings.quota_type;
    if (settings.quota_size) current_setting.quota_size = settings.quota_size;

    return client_->Put(path_, current_setting.ToJsonString());
  }

  Error Remove() const override { return client_->Delete(path_); }

  ReadResult Read(std::string_view entry_name, Time ts) const override { return ReadResult(); }
  Error Write(std::string_view entry_name, std::string_view data, Time ts) const override { return Error(); }
  ListResult List(std::string_view entry_name, Time start, Time stop) const override { return ListResult(); }

 private:
  std::unique_ptr<internal::IHttpClient> client_;
  std::string path_;
};

std::unique_ptr<IBucket> IBucket::Build(std::string_view server_url, std::string_view name) {
  return std::make_unique<Bucket>(server_url, name);
}

// Settings

std::ostream& operator<<(std::ostream& os, const reduct::IBucket::Settings& settings) {
  os << settings.ToJsonString();
  return os;
}

std::string IBucket::Settings::ToJsonString() const noexcept {
  nlohmann::json data;
  if (max_block_size) {
    data["max_block_size"] = *max_block_size;
  }

  if (quota_type) {
    switch (*quota_type) {
      case IBucket::QuotaType::kNone:
        data["quota_type"] = "NONE";
        break;
      case IBucket::QuotaType::kFifo:
        data["quota_type"] = "FIFO";
        break;
    }
  }

  if (quota_size) {
    data["quota_size"] = *quota_size;
  }

  return data.dump();
}

Result<IBucket::Settings> IBucket::Settings::Parse(std::string_view json) noexcept {
  IBucket::Settings settings;
  try {
    auto data = nlohmann::json::parse(json);
    if (data.contains("max_block_size")) {
      settings.max_block_size = data["max_block_size"];
    }

    if (data.contains("quota_type")) {
      if (data["quota_type"] == "NONE") {
        settings.quota_type = IBucket::QuotaType::kNone;
      } else if (data["quota_type"] == "FIFO") {
        settings.quota_type = IBucket::QuotaType::kFifo;
      }
    }

    if (data.contains("quota_size")) {
      settings.quota_size = data["quota_size"];
    }
  } catch (const std::exception& ex) {
    return {{}, Error{.code = -1, .message = ex.what()}};
  }
  return {settings, Error::kOk};
}

}  // namespace reduct