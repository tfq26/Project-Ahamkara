#include "ae/render/gpu_profiler.h"
#include "ae/render/gl_platform.h"
#include "ae/core/log.h"

#include <cstring>

#define AE_LOG_CATEGORY "Render"

namespace ae::render {

GpuProfiler::GpuProfiler() = default;

GpuProfiler::~GpuProfiler() {
    shutdown();
}

bool GpuProfiler::init() {
    // Generate GL query objects.  GL_TIMESTAMP queries require OpenGL 3.3+ or
    // GL_ARB_timer_query.  On macOS the functions are exposed by glext.h and
    // are supported on 10.10+.  If not supported, read_results() will safely
    // report all sections as unavailable.
    const int total_queries = kQueryRingSize * static_cast<int>(kSectionCount) * kQueriesPerSection;

    for (int ring = 0; ring < kQueryRingSize; ++ring) {
        glGenQueries(total_queries / kQueryRingSize,
                     query_ids_[static_cast<std::size_t>(ring)].data());
    }

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        log_warning_cat(AE_LOG_CATEGORY,
                        "GPU profiler: glGenQueries error (0x" + std::to_string(err) + ")");
        supported_ = false;
        return false;
    }

    // Runtime check: try a single GL_TIMESTAMP query to confirm the driver
    // exposes GL_QUERY_COUNTER.
    GLint ext_count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);
    bool has_timer_ext = false;
    for (GLint i = 0; i < ext_count; ++i) {
        const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
        if (ext && std::strstr(ext, "GL_ARB_timer_query") != nullptr) {
            has_timer_ext = true;
            break;
        }
    }

    if (!has_timer_ext) {
        log_warning_cat(AE_LOG_CATEGORY, "GPU profiler: GL_ARB_timer_query not available, GPU profiling disabled");
        supported_ = false;
        return false;
    }

    supported_ = true;
    log_info_cat(AE_LOG_CATEGORY, "GPU profiler initialized with GL timer queries");
    return true;
}

void GpuProfiler::shutdown() {
    if (!supported_)
        return;

    for (auto& ring_queries : query_ids_) {
        glDeleteQueries(static_cast<GLsizei>(kSectionCount * kQueriesPerSection),
                        ring_queries.data());
    }

    supported_ = false;
    current_ring_ = 0;
}

void GpuProfiler::begin_section(GpuProfileSection section) {
    if (!supported_)
        return;

    const std::size_t idx = static_cast<std::size_t>(section) * kQueriesPerSection;
    glQueryCounter(
        query_ids_[static_cast<std::size_t>(current_ring_)][idx],
        GL_TIMESTAMP);
}

void GpuProfiler::end_section(GpuProfileSection section) {
    if (!supported_)
        return;

    const std::size_t idx = static_cast<std::size_t>(section) * kQueriesPerSection + 1;
    glQueryCounter(
        query_ids_[static_cast<std::size_t>(current_ring_)][idx],
        GL_TIMESTAMP);
}

void GpuProfiler::begin_frame() {
    // Advance to next ring slot
    current_ring_ = (current_ring_ + 1) % kQueryRingSize;
}

void GpuProfiler::end_frame() {
    // Nothing to do here — queries were already submitted in begin/end_section.
}

std::array<GpuSectionData, GpuProfiler::kSectionCount> GpuProfiler::read_results() {
    std::array<GpuSectionData, kSectionCount> results {};

    if (!supported_)
        return results;

    // Read results from the oldest ring slot (current - 2, wrapping).
    const std::size_t read_ring =
        static_cast<std::size_t>((current_ring_ + 1) % kQueryRingSize);

    for (std::size_t s = 0; s < kSectionCount; ++s) {
        const std::size_t begin_idx = s * kQueriesPerSection;
        const std::size_t end_idx = s * kQueriesPerSection + 1;

        GLuint64 begin_time = 0;
        GLuint64 end_time = 0;
        GLint begin_ready = 0;
        GLint end_ready = 0;

        glGetQueryObjectiv(query_ids_[read_ring][begin_idx],
                           GL_QUERY_RESULT_AVAILABLE, &begin_ready);
        glGetQueryObjectiv(query_ids_[read_ring][end_idx],
                           GL_QUERY_RESULT_AVAILABLE, &end_ready);

        if (begin_ready && end_ready) {
            glGetQueryObjectui64v(query_ids_[read_ring][begin_idx],
                                  GL_QUERY_RESULT, &begin_time);
            glGetQueryObjectui64v(query_ids_[read_ring][end_idx],
                                  GL_QUERY_RESULT, &end_time);

            if (end_time > begin_time) {
                results[s].elapsed_ms =
                    static_cast<double>(end_time - begin_time) / 1'000'000.0; // ns → ms
                results[s].available = true;
            }
        }
    }

    return results;
}

} // namespace ae::render
