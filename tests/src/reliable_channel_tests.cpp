#include "ae/network/reliable_channel.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(const std::string& message) {
    std::cerr << "reliable_channel_tests failed: " << message << '\n';
    return 1;
}

int test_ack_removes_packets() {
    ae::ReliableChannel rc;
    const ae::u8 p1[] = {1};
    const ae::u8 p2[] = {2};
    const ae::u8 p3[] = {3};
    rc.on_send(1, p1, sizeof(p1), 0.0);
    rc.on_send(2, p2, sizeof(p2), 0.0);
    rc.on_send(3, p3, sizeof(p3), 0.0);
    if (rc.pending_count() != 3) return fail("expected 3 pending");

    // Peer acks seq 2 directly and seq 1 via bit 0 (ack_sequence - 1 - 0 = 1).
    rc.on_ack(2, 0x1u);
    if (rc.pending_count() != 1) return fail("ack should leave only seq 3");
    if (rc.payload(1) != nullptr) return fail("seq 1 should be acked/removed");
    if (rc.payload(3) == nullptr) return fail("seq 3 should still be pending");
    return 0;
}

int test_retransmit_timeout() {
    ae::ReliableChannel rc;
    const ae::u8 p[] = {9};
    rc.on_send(7, p, sizeof(p), 0.0);

    if (!rc.collect_retransmits(0.05, 0.1).empty()) {
        return fail("not yet timed out");
    }
    auto due = rc.collect_retransmits(0.2, 0.1);
    if (due.size() != 1 || due[0] != 7) return fail("seq 7 should be due for resend");
    if (rc.send_count(7) != 2) return fail("send_count should increment on resend");
    // After refreshing send time, not immediately due again.
    if (!rc.collect_retransmits(0.25, 0.1).empty()) {
        return fail("should not be due again right after refresh");
    }
    rc.on_ack(7, 0x0u);
    if (rc.pending_count() != 0) return fail("acking 7 should empty the channel");
    return 0;
}

int test_ack_wraparound() {
    ae::ReliableChannel rc;
    const ae::u8 p[] = {0};
    rc.on_send(65535, p, sizeof(p), 0.0);
    // ack_sequence = 0, bit 0 acks (0 - 1 - 0) wrapped = 65535.
    rc.on_ack(0, 0x1u);
    if (rc.pending_count() != 0) return fail("wraparound ack should remove seq 65535");
    return 0;
}

}  // namespace

int main() {
    if (int rc = test_ack_removes_packets(); rc != 0) return rc;
    if (int rc = test_retransmit_timeout(); rc != 0) return rc;
    if (int rc = test_ack_wraparound(); rc != 0) return rc;
    std::cout << "reliable_channel_tests passed\n";
    return 0;
}
