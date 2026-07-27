#pragma once

#include "wish/types.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace wish::session {

// ---------------------------------------------------------------------------
// PartyMemberState
// ---------------------------------------------------------------------------
enum class PartyMemberState : wish::u8 {
    Online,
    Ready,
    Away,
    InMatchmaking
};

// ---------------------------------------------------------------------------
// PartyMember
// ---------------------------------------------------------------------------
struct PartyMember {
    wish::NetAddress address {};
    std::string identity {};
    PartyMemberState state {PartyMemberState::Online};
    std::chrono::steady_clock::time_point joined_at {};
    bool is_ready {false};
};

// ---------------------------------------------------------------------------
// Party — pre-game player grouping with leader, ready states, join/leave flow
// ---------------------------------------------------------------------------
class Party {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    /// Create a new party with the given id and initial leader.
    /// The leader is added as the first member automatically.
    Party(wish::u64 party_id, const wish::NetAddress& leader_address, time_point now,
          wish::u32 max_members = 8);

    // -- Membership -----------------------------------------------------------

    /// Add a member to the party. Returns pointer to the new member,
    /// or nullptr if the party is full or the member already exists.
    PartyMember* add_member(const wish::NetAddress& addr, time_point now);

    /// Remove a member from the party. Returns true if the member was found
    /// and removed. If the removed member was the leader, leadership is
    /// transferred to the longest-standing remaining member (or the party
    /// becomes leaderless if empty).
    bool remove_member(const wish::NetAddress& addr);

    /// Find a member by address. Returns nullptr if not found.
    PartyMember* find_member(const wish::NetAddress& addr);

    /// Find a member by address (const).
    const PartyMember* find_member(const wish::NetAddress& addr) const;

    /// Return true if the party has no members.
    [[nodiscard]] bool empty() const;

    /// Return the number of members.
    [[nodiscard]] wish::u32 member_count() const;

    /// Return the maximum number of members allowed.
    [[nodiscard]] wish::u32 max_members() const;

    /// Set a new maximum member count.
    void set_max_members(wish::u32 max);

    /// Return true if member_count() >= max_members().
    [[nodiscard]] bool is_full() const;

    // -- Leadership -----------------------------------------------------------

    /// Return the address of the current leader.
    /// If the party is empty, returns a default-constructed NetAddress.
    [[nodiscard]] wish::NetAddress leader() const;

    /// Transfer leadership to a different member. Returns false if the
    /// target is not a member.
    bool transfer_leadership(const wish::NetAddress& new_leader);

    /// Check if the given address belongs to the leader.
    [[nodiscard]] bool is_leader(const wish::NetAddress& addr) const;

    // -- Ready state ----------------------------------------------------------

    /// Set a member's ready flag. Returns false if the member was not found.
    bool set_member_ready(const wish::NetAddress& addr, bool ready);

    /// Return true if all members are ready (or party has no members).
    [[nodiscard]] bool all_ready() const;

    /// Return true if the member at `addr` is ready.
    [[nodiscard]] bool is_ready(const wish::NetAddress& addr) const;

    // -- Matchmaking integration ----------------------------------------------

    /// Mark the party as having entered matchmaking (or exited).
    void set_in_matchmaking(bool value);

    /// Return true if this party is currently in the matchmaking queue.
    [[nodiscard]] bool in_matchmaking() const;

    // -- Identity -------------------------------------------------------------

    /// Return the unique party identifier.
    [[nodiscard]] wish::u64 party_id() const { return party_id_; }

    // -- Enumeration ----------------------------------------------------------

    template <typename Fn>
    void for_each_member(Fn&& fn) {
        for (auto& member : members_) {
            fn(member);
        }
    }

    template <typename Fn>
    void for_each_member(Fn&& fn) const {
        for (const auto& member : members_) {
            fn(member);
        }
    }

  private:
    static bool same_address(const wish::NetAddress& lhs, const wish::NetAddress& rhs);

    wish::u64 party_id_;
    wish::u32 max_members_;
    wish::NetAddress leader_address_;
    std::vector<PartyMember> members_;
    bool in_matchmaking_ {false};
};

} // namespace wish::session
