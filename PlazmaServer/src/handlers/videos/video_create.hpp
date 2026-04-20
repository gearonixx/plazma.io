#pragma once

#include <string>

#include <userver/engine/task/task_processor_fwd.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/scylla/component.hpp>
#include "../../s3/component.hpp"

namespace real_medium::handlers::videos::create {

class Handler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-videos-create";

    Handler(const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context);

    std::string HandleRequest(
        userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context
    ) const override;

private:
    // Store a user-supplied (optimistic) thumbnail upfront, then kick off the
    // authoritative ffmpeg-based extraction asynchronously. Called from
    // HandleRequest once the main object is in S3 and metadata is in Scylla.
    void SeedOptimisticThumbnail(const std::string& video_id,
                                 const std::string& thumb_bytes,
                                 const std::string& thumb_mime) const;

    void ScheduleMediaDerivatives(std::string video_bytes,
                                  std::string mime,
                                  std::string video_id,
                                  int64_t user_id,
                                  std::string day,
                                  std::string visibility,
                                  bool skip_primary_thumb) const;

    s3::S3Component& s3_;
    userver::storages::scylla::SessionPtr session_;
    userver::engine::TaskProcessor& fs_tp_;
};

}
