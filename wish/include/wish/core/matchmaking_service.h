#pragma once

#include "wish/types.h"
#include "wish/session/party.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace wish::core {

// ---------------------------------------------------------------------------
// Fireteam — result of matching one or more parties together; represents the
// group that will be placed into a single activity session.
// ---------------------------------------------------------------------------
struct Fireteam {
    wish::u64 fireteam_id {0};
    wish::u64 activity_id {0};
    std::vector<wish::u64> party_ids {};
    std::vector<wish::NetAddress> member_addresses {};
    std::chrono::steady_clock::time_point formed_at {};
    bool assigned_to_session {false};

    [[nodiscard]] wish::u32 member_count() const {
        return static_cast<wish::u32>(member_addresses.size());
    }

    [[nodiscard]] bool empty() const {
        return member_addresses.empty();
    }
};

// ---------------------------------------------------------------------------
// MatchmakingTicket — represents a party's position in the matchmaking queue.
// ---------------------------------------------------------------------------
struct MatchmakingTicket {
    wish::u64 ticket_id {0};
    wish::u64 party_id {0};
    wish::u64 activity_id {0};
    wish::u32 party_size {0};
    std::chrono::steady_clock::time_point queued_at {};
    bool active {false};
};

// ---------------------------------------------------------------------------
// MatchmakingService — queues parties and matches them together into
// fireteams that can be assigned to activity sessions.
// ---------------------------------------------------------------------------
class MatchmakingService {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    // Callback type invoked when a match is formed.
    // Receives the fireteam that was created from matched parties.
    using MatchFoundCallback = std::function<void(const Fireteam& fireteam)>;

    MatchmakingService() = default;

    // -- Queue management -----------------------------------------------------

    /// Place a party into the matchmaking queue for the given activity.
    /// Returns a ticket id that can be used to track or cancel the request.
    /// Returns 0 if the party is empty.
    [[nodiscard]] wish::u64 enqueue_party(wish::u64 party_id, wish::u32 party_size,
                                          wish::u64 activity_id, time_point now);

    /// Remove a party from the queue by ticket id.
    /// Returns true if the ticket was found and dequeued.
    bool dequeue_ticket(wish::u64 ticket_id);

    /// Remove all tickets belonging to a given party.
    /// Returns the number of tickets removed.
    wish::u32 dequeue_party(wish::u64 party_id);

    // -- Matching -------------------------------------------------------------

    /// Run one matchmaking tick: scan the queue and form fireteams when
    /// enough players are available for an activity. The formed fireteams
    /// are delivered via the match-found callback.
    void tick(time_point now);

    /// Set the callback that fires when a match is formed.
    void set_on_match_found(MatchFoundCallback cb) {
        on_match_found_ = std::move(cb);
    }

    // -- Configuration --------------------------------------------------------

    /// Set the target player count per activity. The service will attempt
    /// to match parties until this many players are accumulated per fireteam.
    void set_target_players_per_match(wish::u32 count) {
        target_players_per_match_ = count;
    }

    [[nodiscard]] wish::u32 target_players_per_match() const {
        return target_players_per_match_;
    }

    // -- Query ----------------------------------------------------------------

    /// Return the number of active tickets currently in the queue.
    [[nodiscard]] wish::u32 queue_size() const;

    /// Return the total number of players currently in the queue.
    [[nodiscard]] wish::u32 queued_player_count() const;

    /// Return the number of fireteams formed so far.
    [[nodiscard]] wish::u64 fireteam_count() const {
        return next_fireteam_id_ - 1;
    }

    /// Return the number of tickets processed (matched or failed).
    [[nodiscard]] wish::u64 tickets_processed() const {
        return tickets_processed_;
    }

    // -- Inspection -----------------------------------------------------------

    /// Find a ticket by ticket id (const).
    const MatchmakingTicket* find_ticket(wish::u64 ticket_id) const;

    /// Find a ticket by party id (const). Returns nullptr if the party
    /// has no active ticket.
    const MatchmakingTicket* find_ticket_by_party(wish::u64 party_id) const;

  private:
    /// Gather parties from the queue that are queued for the given activity.
    void collect_available_parties(wish::u64 activity_id,
                                   std::vector<MatchmakingTicket*>& candidates);

    /// Try to form a fireteam from a set of queued tickets.
    bool try_form_fireteam(const std::vector<MatchmakingTicket*>& candidates,
                           wish::u64 activity_id, time_point now);

    std::vector<MatchmakingTicket> tickets_;
    wish::u64 next_ticket_id_ {1};
    wish::u64 next_fireteam_id_ {1};
    wish::u64 tickets_processed_ {0};
    wish::u32 target_players_per_match_ {8};
    MatchFoundCallback on_match_found_ {};
};

} // namespace wish::core
