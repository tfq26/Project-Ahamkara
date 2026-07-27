#include "wish/session/party.h"

namespace wish::session {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
bool Party::same_address(const wish::NetAddress& lhs, const wish::NetAddress& rhs) {
    return lhs.port == rhs.port && lhs.ip == rhs.ip;
}

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
Party::Party(wish::u64 party_id, const wish::NetAddress& leader_address, time_point now,
             wish::u32 max_members)
    : party_id_(party_id), max_members_(max_members), leader_address_(leader_address) {
    // Leader is automatically the first member
    members_.push_back(PartyMember{leader_address,
                                   leader_address.ip + ":" + std::to_string(leader_address.port),
                                   PartyMemberState::Online,
                                   now,
                                   false});
}

// ---------------------------------------------------------------------------
// membership
// ---------------------------------------------------------------------------
PartyMember* Party::add_member(const wish::NetAddress& addr, time_point now) {
    // Reject if the address is already in the party
    if (find_member(addr) != nullptr) {
        return nullptr;
    }

    if (is_full()) {
        return nullptr;
    }

    members_.push_back(PartyMember{addr,
                                   addr.ip + ":" + std::to_string(addr.port),
                                   PartyMemberState::Online,
                                   now,
                                   false});
    return &members_.back();
}

bool Party::remove_member(const wish::NetAddress& addr) {
    const auto it = std::find_if(members_.begin(), members_.end(), [&](const PartyMember& m) {
        return same_address(m.address, addr);
    });

    if (it == members_.end()) {
        return false;
    }

    const bool was_leader = same_address(leader_address_, addr);
    members_.erase(it);

    // If the leader left, transfer leadership to the longest-standing member
    if (was_leader && !members_.empty()) {
        // First member (earliest joined) becomes the new leader
        leader_address_ = members_.front().address;
    }

    return true;
}

PartyMember* Party::find_member(const wish::NetAddress& addr) {
    const auto it = std::find_if(members_.begin(), members_.end(), [&](const PartyMember& m) {
        return same_address(m.address, addr);
    });
    return it == members_.end() ? nullptr : &(*it);
}

const PartyMember* Party::find_member(const wish::NetAddress& addr) const {
    const auto it = std::find_if(members_.begin(), members_.end(), [&](const PartyMember& m) {
        return same_address(m.address, addr);
    });
    return it == members_.end() ? nullptr : &(*it);
}

bool Party::empty() const {
    return members_.empty();
}

wish::u32 Party::member_count() const {
    return static_cast<wish::u32>(members_.size());
}

wish::u32 Party::max_members() const {
    return max_members_;
}

void Party::set_max_members(wish::u32 max) {
    max_members_ = max;
}

bool Party::is_full() const {
    return members_.size() >= max_members_;
}

// ---------------------------------------------------------------------------
// leadership
// ---------------------------------------------------------------------------
wish::NetAddress Party::leader() const {
    if (members_.empty()) {
        return {};
    }
    return leader_address_;
}

bool Party::transfer_leadership(const wish::NetAddress& new_leader) {
    if (find_member(new_leader) == nullptr) {
        return false;
    }
    leader_address_ = new_leader;
    return true;
}

bool Party::is_leader(const wish::NetAddress& addr) const {
    return same_address(leader_address_, addr);
}

// ---------------------------------------------------------------------------
// ready state
// ---------------------------------------------------------------------------
bool Party::set_member_ready(const wish::NetAddress& addr, bool ready) {
    auto* member = find_member(addr);
    if (member == nullptr) {
        return false;
    }
    member->is_ready = ready;
    member->state = ready ? PartyMemberState::Ready : PartyMemberState::Online;
    return true;
}

bool Party::all_ready() const {
    if (members_.empty()) {
        return true;
    }
    return std::all_of(members_.begin(), members_.end(),
                       [](const PartyMember& m) { return m.is_ready; });
}

bool Party::is_ready(const wish::NetAddress& addr) const {
    const auto* member = find_member(addr);
    return member != nullptr && member->is_ready;
}

// ---------------------------------------------------------------------------
// matchmaking integration
// ---------------------------------------------------------------------------
void Party::set_in_matchmaking(bool value) {
    in_matchmaking_ = value;
    if (value) {
        for (auto& member : members_) {
            member.state = PartyMemberState::InMatchmaking;
        }
    }
}

bool Party::in_matchmaking() const {
    return in_matchmaking_;
}

} // namespace wish::session
