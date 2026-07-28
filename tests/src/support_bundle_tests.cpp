#include "wish/core/support_bundle.h"
#include "wish/core/observability.h"
#include "wish/core/error_codes.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

namespace {

void test_generate_bundle_id() {
    auto id1 = wish::core::generate_bundle_id();
    auto id2 = wish::core::generate_bundle_id();

    assert(id1.size() == 20);
    assert(id1.find("BND-") == 0);
    assert(id1 != id2);

    std::cout << "test_generate_bundle_id passed.\n";
}

void test_operation_history_record_and_find() {
    wish::core::OperationHistory history;

    wish::core::OperationRecord op;
    op.operation_id = "op-auth-001";
    op.operation_name = "auth.validate";
    op.outcome = wish::core::OperationOutcome::Success;
    op.timestamp = std::chrono::system_clock::now();

    history.record_operation(op);

    assert(history.operation_count() == 1);
    assert(history.has_operation_id("op-auth-001"));
    assert(!history.has_operation_id("op-nonexistent"));

    const auto* found = history.find_operation("op-auth-001");
    assert(found != nullptr);
    assert(found->operation_name == "auth.validate");
    assert(found->outcome == wish::core::OperationOutcome::Success);

    std::cout << "test_operation_history_record_and_find passed.\n";
}

void test_operation_history_idempotency() {
    wish::core::OperationHistory history;

    wish::core::OperationRecord op;
    op.operation_id = "op-admit-001";
    op.operation_name = "session.admit";
    op.outcome = wish::core::OperationOutcome::Success;
    op.timestamp = std::chrono::system_clock::now();

    history.record_operation(op);

    assert(!history.should_retry("op-admit-001"));

    assert(history.should_retry("op-unknown"));

    std::cout << "test_operation_history_idempotency passed.\n";
}

void test_operation_retry_tracking() {
    wish::core::OperationHistory history;

    wish::core::OperationRecord op;
    op.operation_id = "op-retry-001";
    op.operation_name = "backend.connect";
    op.outcome = wish::core::OperationOutcome::Failed;
    op.timestamp = std::chrono::system_clock::now();
    op.retry_count = 1;
    op.max_retries = 3;
    op.error_code = 5001;

    history.record_operation(op);

    assert(history.should_retry("op-retry-001"));

    wish::core::OperationRecord op2;
    op2.operation_id = "op-retry-002";
    op2.operation_name = "backend.connect";
    op2.outcome = wish::core::OperationOutcome::Failed;
    op2.timestamp = std::chrono::system_clock::now();
    op2.retry_count = 5;
    op2.max_retries = 3;
    op2.error_code = 5001;

    history.record_operation(op2);

    assert(!history.should_retry("op-retry-002"));

    std::cout << "test_operation_retry_tracking passed.\n";
}

void test_operation_abandoned_not_retried() {
    wish::core::OperationHistory history;

    wish::core::OperationRecord op;
    op.operation_id = "op-abandoned";
    op.operation_name = "session.admit";
    op.outcome = wish::core::OperationOutcome::Abandoned;

    history.record_operation(op);

    assert(!history.should_retry("op-abandoned"));

    std::cout << "test_operation_abandoned_not_retried passed.\n";
}

void test_build_support_bundle() {
    wish::core::ServiceIdentity service;
    service.service_name = "wish-engine";
    service.instance_id = "srv-001";
    service.version = "0.1.0";

    wish::core::OperationHistory history;

    wish::core::OperationRecord op;
    op.operation_id = "op-001";
    op.operation_name = "auth.validate";
    op.outcome = wish::core::OperationOutcome::Success;
    history.record_operation(op);

    std::vector<wish::core::CorrelatedFailure> failures;
    failures.push_back(wish::core::correlate_failure(
        wish::WishErrorCode::kBackendTimeout, 0, "wish/integrations",
        "Backend timeout during auth", true));

    auto bundle = wish::core::build_support_bundle(
        service, "0.1.0", 3600, failures, history, "Test diagnostic bundle");

    assert(!bundle.bundle_id.empty());
    assert(bundle.service.service_name == "wish-engine");
    assert(bundle.server_version == "0.1.0");
    assert(bundle.uptime_seconds == 3600);
    assert(bundle.failures.size() == 1);
    assert(bundle.operations.size() == 1);
    assert(bundle.notes == "Test diagnostic bundle");

    std::cout << "test_build_support_bundle passed.\n";
}

void test_format_support_bundle() {
    wish::core::ServiceIdentity service;
    service.service_name = "wish-engine";
    service.instance_id = "srv-001";
    service.version = "0.1.0";

    auto bundle = wish::core::build_support_bundle(
        service, "0.1.0", 7200, {}, wish::core::OperationHistory{}, "Test notes");

    auto formatted = wish::core::format_support_bundle(bundle);
    assert(!formatted.empty());
    assert(formatted.find(bundle.bundle_id) != std::string::npos);
    assert(formatted.find("wish-engine") != std::string::npos);
    assert(formatted.find("0.1.0") != std::string::npos);
    assert(formatted.find("7200s") != std::string::npos);
    assert(formatted.find("Test notes") != std::string::npos);
    assert(formatted.find("=== End Bundle ===") != std::string::npos);

    std::cout << "test_format_support_bundle passed.\n";
}

void test_operation_record_formatting() {
    wish::core::OperationRecord op;
    op.operation_id = "op-auth-001";
    op.operation_name = "auth.validate";
    op.outcome = wish::core::OperationOutcome::Success;
    op.retry_count = 0;
    op.max_retries = 3;

    auto formatted = wish::core::format_operation_record(op);
    assert(formatted.find("op-auth-001") != std::string::npos);
    assert(formatted.find("SUCCESS") != std::string::npos);

    std::cout << "test_operation_record_formatting passed.\n";
}

void test_idempotent_replay_detection() {
    wish::core::OperationHistory history;

    wish::core::OperationRecord original;
    original.operation_id = "op-idempotent";
    original.operation_name = "session.admit";
    original.outcome = wish::core::OperationOutcome::Success;
    history.record_operation(original);

    wish::core::OperationRecord replay;
    replay.operation_id = "op-idempotent";
    replay.operation_name = "session.admit";
    replay.outcome = wish::core::OperationOutcome::IdempotentReplay;
    replay.is_idempotent_replay = true;
    history.record_operation(replay);

    assert(history.operation_count() == 2);

    const auto* found = history.find_operation("op-idempotent");
    assert(found != nullptr);

    auto formatted = wish::core::format_operation_record(*found);
    assert(formatted.find("IDEMPOTENT_REPLAY") != std::string::npos ||
           formatted.find("[IDEMPOTENT]") != std::string::npos);

    std::cout << "test_idempotent_replay_detection passed.\n";
}

void test_history_max_limit() {
    wish::core::OperationHistory history;

    for (std::size_t i = 0; i < wish::core::kMaxOperationHistory + 10; ++i) {
        wish::core::OperationRecord op;
        op.operation_id = "op-" + std::to_string(i);
        op.operation_name = "test";
        op.outcome = wish::core::OperationOutcome::Success;
        history.record_operation(op);
    }

    assert(history.operation_count() <= wish::core::kMaxOperationHistory);

    std::cout << "test_history_max_limit passed.\n";
}

void test_clear_history() {
    wish::core::OperationHistory history;

    wish::core::OperationRecord op;
    op.operation_id = "op-001";
    op.operation_name = "test";
    op.outcome = wish::core::OperationOutcome::Success;
    history.record_operation(op);

    assert(history.operation_count() == 1);

    history.clear();
    assert(history.operation_count() == 0);

    std::cout << "test_clear_history passed.\n";
}

} // namespace

int main() {
    test_generate_bundle_id();
    test_operation_history_record_and_find();
    test_operation_history_idempotency();
    test_operation_retry_tracking();
    test_operation_abandoned_not_retried();
    test_build_support_bundle();
    test_format_support_bundle();
    test_operation_record_formatting();
    test_idempotent_replay_detection();
    test_history_max_limit();
    test_clear_history();

    std::cout << "All support bundle tests passed.\n";
    return 0;
}
