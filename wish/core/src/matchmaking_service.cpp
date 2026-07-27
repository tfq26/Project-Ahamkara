#include "wish/core/matchmaking_service.h"
#include "wish/log.h"

#include <algorithm>
#include <numeric>

namespace wish::core {

// ---------------------------------------------------------------------------
// queue management
// ---------------------------------------------------------------------------
wish::u64 MatchmakingService::enqueue_party(wish::u64 party_id, wish::u32 party_size,
                                            wish::u64 activity_id, time_point now) {
    if (party_size == 0) {
        wish::log_warning("MatchmakingService: cannot enqueue empty party (id=" +
                          std::to_string(party_id) + ")");
        return 0;
    }

    // Check if the party is already queued
    if (find_ticket_by_party(party_id) != nullptr) {
        wish::log_warning("MatchmakingService: party " + std::to_string(party_id) +
                          " is already queued");
        return 0;
    }

    const auto ticket_id = next_ticket_id_++;
    tickets_.push_back(MatchmakingTicket{ticket_id, party_id, activity_id,
                                         party_size, now, true});

    wish::log_info("MatchmakingService: party " + std::to_string(party_id) +
                   " (" + std::to_string(party_size) + " players) enqueued for activity " +
                   std::to_string(activity_id) + " as ticket " + std::to_string(ticket_id));

    return ticket_id;
}

bool MatchmakingService::dequeue_ticket(wish::u64 ticket_id) {
    const auto it = std::find_if(tickets_.begin(), tickets_.end(),
                                 [ticket_id](const MatchmakingTicket& t) {
                                     return t.ticket_id == ticket_id && t.active;
                                 });

    if (it == tickets_.end()) {
        return false;
    }

    it->active = false;
    ++tickets_processed_;
    wish::log_info("MatchmakingService: ticket " + std::to_string(ticket_id) +
                   " removed from queue");
    return true;
}

wish::u32 MatchmakingService::dequeue_party(wish::u64 party_id) {
    wish::u32 removed = 0;
    for (auto& ticket : tickets_) {
        if (ticket.party_id == party_id && ticket.active) {
            ticket.active = false;
            ++removed;
            ++tickets_processed_;
        }
    }
    if (removed > 0) {
        wish::log_info("MatchmakingService: party " + std::to_string(party_id) +
                       " removed with " + std::to_string(removed) + " ticket(s)");
    }
    return removed;
}

// ---------------------------------------------------------------------------
// matching
// ---------------------------------------------------------------------------
void MatchmakingService::collect_available_parties(
    wish::u64 activity_id, std::vector<MatchmakingTicket*>& candidates) {
    for (auto& ticket : tickets_) {
        if (ticket.active && ticket.activity_id == activity_id) {
            candidates.push_back(&ticket);
        }
    }
}

bool MatchmakingService::try_form_fireteam(
    const std::vector<MatchmakingTicket*>& candidates,
    wish::u64 activity_id, time_point now) {

    if (candidates.empty()) {
        return false;
    }

    // Accumulate parties until we meet or exceed the target player count.
    // This uses a first-come-first-served approach within each activity queue.
    std::vector<MatchmakingTicket*> selected;
    wish::u32 accumulated = 0;

    for (auto* ticket : candidates) {
        if (!ticket->active) continue;
        selected.push_back(ticket);
        accumulated += ticket->party_size;
        if (accumulated >= target_players_per_match_) {
            break;
        }
    }

    // Only form a fireteam if we have at least 2 parties or a full party
    // For single parties, only match if they meet the target (full party).
    if (accumulated < target_players_per_match_ && selected.size() < 2) {
        // Not enough players to form a match yet
        return false;
    }

    // Build the fireteam
    Fireteam fireteam;
    fireteam.fireteam_id = next_fireteam_id_++;
    fireteam.activity_id = activity_id;
    fireteam.formed_at = now;

    // We still form a match even if slightly under target, but log it
    if (accumulated < target_players_per_match_) {
        wish::log_info("MatchmakingService: forming underfull fireteam #" +
                       std::to_string(fireteam.fireteam_id) + " with " +
                       std::to_string(accumulated) + "/" +
                       std::to_string(target_players_per_match_) + " players");
    }

    for (auto* ticket : selected) {
        ticket->active = false;
        ++tickets_processed_;
        fireteam.party_ids.push_back(ticket->party_id);
        // Member addresses are resolved by the caller who has access
        // to the actual Party objects; we store party_ids for lookup.
    }

    wish::log_info("MatchmakingService: fireteam #" + std::to_string(fireteam.fireteam_id) +
                   " formed with " + std::to_string(selected.size()) + " party/ies (" +
                   std::to_string(accumulated) + " players) for activity " +
                   std::to_string(activity_id));

    // Fire callback
    if (on_match_found_) {
        on_match_found_(fireteam);
    }

    return true;
}

void MatchmakingService::tick(time_point now) {
    if (tickets_.empty()) {
        return;
    }

    // Collect unique activity ids from active tickets
    std::vector<wish::u64> active_activities;
    for (const auto& ticket : tickets_) {
        if (!ticket.active) continue;
        if (std::find(active_activities.begin(), active_activities.end(),
                      ticket.activity_id) == active_activities.end()) {
            active_activities.push_back(ticket.activity_id);
        }
    }

    // Attempt to form matches per activity
    for (const auto activity_id : active_activities) {
        std::vector<MatchmakingTicket*> candidates;
        collect_available_parties(activity_id, candidates);

        // Sort by queue time (FCFS)
        std::sort(candidates.begin(), candidates.end(),
                  [](const MatchmakingTicket* a, const MatchmakingTicket* b) {
                      return a->queued_at < b->queued_at;
                  });

        // Keep trying to form fireteams until no more matches can be made
        bool formed = true;
        while (formed) {
            // Re-collect available (non-matched) candidates
            candidates.clear();
            collect_available_parties(activity_id, candidates);
            std::sort(candidates.begin(), candidates.end(),
                      [](const MatchmakingTicket* a, const MatchmakingTicket* b) {
                          return a->queued_at < b->queued_at;
                      });
            formed = try_form_fireteam(candidates, activity_id, now);
        }
    }
}

// ---------------------------------------------------------------------------
// queries
// ---------------------------------------------------------------------------
wish::u32 MatchmakingService::queue_size() const {
    return static_cast<wish::u32>(
        std::count_if(tickets_.begin(), tickets_.end(),
                      [](const MatchmakingTicket& t) { return t.active; }));
}

wish::u32 MatchmakingService::queued_player_count() const {
    return static_cast<wish::u32>(
        std::accumulate(tickets_.begin(), tickets_.end(), 0u,
                        [](wish::u32 sum, const MatchmakingTicket& t) {
                            return sum + (t.active ? t.party_size : 0);
                        }));
}

const MatchmakingTicket* MatchmakingService::find_ticket(wish::u64 ticket_id) const {
    const auto it = std::find_if(tickets_.begin(), tickets_.end(),
                                 [ticket_id](const MatchmakingTicket& t) {
                                     return t.ticket_id == ticket_id;
                                 });
    return it == tickets_.end() ? nullptr : &(*it);
}

const MatchmakingTicket* MatchmakingService::find_ticket_by_party(wish::u64 party_id) const {
    const auto it = std::find_if(tickets_.begin(), tickets_.end(),
                                 [party_id](const MatchmakingTicket& t) {
                                     return t.party_id == party_id && t.active;
                                 });
    return it == tickets_.end() ? nullptr : &(*it);
}

} // namespace wish::core
