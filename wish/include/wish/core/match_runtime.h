#pragma once

#include "ae/core/types.h"
#include "ae/network/packet_envelope.h"
#include "wish/replication/replication_frame.h"
#include "wish/session/session_model.h"

#include <utility>

namespace wish::core {

template <typename AuthorityState, typename SnapshotPayload>
struct IMatchRuntimeHooks {
    virtual ~IMatchRuntimeHooks() = default;

    virtual bool validate_fire(const AuthorityState& authority, const SnapshotPayload& snapshot) const = 0;
    virtual void apply_damage(AuthorityState& authority, SnapshotPayload& snapshot) = 0;
    virtual void schedule_respawn(AuthorityState& authority, SnapshotPayload& snapshot) = 0;
    virtual void update_scoreboard(AuthorityState& authority, SnapshotPayload& snapshot) = 0;
    virtual void tick_match_timer(AuthorityState& authority, SnapshotPayload& snapshot, float delta_seconds) = 0;
};

template <typename AuthorityState, typename SnapshotPayload>
struct NullMatchRuntimeHooks final : IMatchRuntimeHooks<AuthorityState, SnapshotPayload> {
    bool validate_fire(const AuthorityState&, const SnapshotPayload&) const override {
        return true;
    }

    void apply_damage(AuthorityState&, SnapshotPayload&) override {}
    void schedule_respawn(AuthorityState&, SnapshotPayload&) override {}
    void update_scoreboard(AuthorityState&, SnapshotPayload&) override {}
    void tick_match_timer(AuthorityState&, SnapshotPayload&, float) override {}
};

template <typename AuthorityState, typename SnapshotPayload, typename HistoryBuffer>
class MatchRuntime {
public:
    using authority_state_type = AuthorityState;
    using snapshot_payload_type = SnapshotPayload;
    using history_buffer_type = HistoryBuffer;
    using hooks_type = IMatchRuntimeHooks<AuthorityState, SnapshotPayload>;

    explicit MatchRuntime(hooks_type* hooks = nullptr)
        : hooks_(hooks != nullptr ? hooks : &null_hooks()) {
    }

    [[nodiscard]] hooks_type& hooks() {
        return *hooks_;
    }

    [[nodiscard]] const hooks_type& hooks() const {
        return *hooks_;
    }

    void set_hooks(hooks_type* hooks) {
        hooks_ = hooks != nullptr ? hooks : &null_hooks();
    }

    void note_client(const ae::NetAddress& address, ae::u32 received_input_sequence, const ae::PacketEnvelope& envelope) {
        session.client_address = address;
        session.connected = true;
        session.sequence_tracker.process_incoming(envelope);
        session.last_received_input_sequence = received_input_sequence;
    }

    void mark_input_processed(ae::u32 input_sequence) {
        session.last_processed_input_sequence = input_sequence;
        replication.last_processed_input = input_sequence;
    }

    [[nodiscard]] ae::PacketEnvelope prepare_snapshot_envelope() {
        return session.sequence_tracker.prepare_outgoing();
    }

    template <typename InputCommand, typename TickFn>
    void advance_authoritative(float delta_seconds, const InputCommand& input, TickFn&& tick_fn) {
        std::forward<TickFn>(tick_fn)(authoritative, delta_seconds, input);
    }

    template <typename SnapshotFn, typename HistoryFn>
    void commit_replication(SnapshotFn&& build_snapshot, HistoryFn&& record_history) {
        ++session.server_tick;
        ++replication.sequence;
        replication.server_tick = session.server_tick;
        replication.last_processed_input = session.last_processed_input_sequence;
        replication.authoritative = true;
        std::forward<SnapshotFn>(build_snapshot)(
            authoritative,
            replication.snapshot,
            session.server_tick,
            session.last_processed_input_sequence
        );
        std::forward<HistoryFn>(record_history)(authoritative, history, session.server_tick);
    }

    [[nodiscard]] bool has_client() const {
        return session.connected;
    }

    void clear_client() {
        session.connected = false;
    }

    session::SessionModel session {};
    AuthorityState authoritative {};
    replication::ReplicationFrame<SnapshotPayload> replication {};
    HistoryBuffer history {};

private:
    static NullMatchRuntimeHooks<AuthorityState, SnapshotPayload>& null_hooks() noexcept {
        static NullMatchRuntimeHooks<AuthorityState, SnapshotPayload> hooks {};
        return hooks;
    }

    hooks_type* hooks_ {nullptr};
};

}  // namespace wish::core
