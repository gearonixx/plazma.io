#include "component.hpp"

#include <chrono>
#include <userver/components/component_context.hpp>

namespace real_medium::s3 {

namespace {

constexpr auto kTimeout = std::chrono::seconds{30};
constexpr int kRetryCount = 3;
constexpr auto kMinioEndpoint = "minio:9000";
constexpr auto kBucket = "plazma-videos";

}  // namespace

S3Component::S3Component(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
) : userver::components::LoggableComponentBase(config, context),
    http_client_(context.FindComponent<userver::components::HttpClient>().GetHttpClient()),
    client_([this] {
        auto connection = userver::s3api::MakeS3Connection(
            http_client_,
            userver::s3api::S3ConnectionType::kHttp,
            kMinioEndpoint,
            userver::s3api::ConnectionCfg{kTimeout, kRetryCount, std::nullopt}
        );
        auto auth = std::make_shared<userver::s3api::authenticators::AccessKey>(
            "minioadmin", userver::s3api::Secret("minioadmin")
        );
        return userver::s3api::GetS3Client(std::move(connection), std::move(auth), kBucket);
    }()) {
}

userver::s3api::ClientPtr S3Component::GetClient() const {
    return client_;
}

}
