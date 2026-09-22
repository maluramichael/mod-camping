/*
 * mod-camping - shared declarations.
 *
 * A campfire rest spot: `.camp` summons an existing "Basic Campfire" gameobject at the
 * player's feet; standing near any active campfire (this fork's own, or any other Basic
 * Campfire already placed in the world) grants a timed "Camp Rest" aura. See README.md for
 * the full design and the v1 scope cut.
 *
 * Released under GNU GPL v2; redistribute/modify under version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef MOD_CAMPING_H
#define MOD_CAMPING_H

#include "Define.h"

namespace Camping
{
    // Gameobject_template entry for Blizzard's "Basic Campfire" - verified in-tree: it is the
    // exact entry `pet_generic.cpp` (Plump Turkey pet AI) already searches for with
    // FindNearestGameObject(GO_BASIC_CAMPFIRE, 7.0f), so the world DB is guaranteed to ship it.
    constexpr uint32 GO_BASIC_CAMPFIRE = 29784;

    // Server-side "Camp Rest" carrier aura, created by this module's SQL (see
    // data/sql/db-world/updates/mod_camping_*.sql), same pattern as mod-group-buffs.
    constexpr uint32 SPELL_CAMP_REST = 200240;

    struct Config
    {
        bool enable = true;
        uint32 buffDurationMin = 60;      // Camp Rest aura duration while refreshed, in minutes
        uint32 summonCooldownSec = 300;   // per-player cooldown on `.camp`
        float restRangeYards = 15.0f;     // proximity range for the rest buff
        uint32 campfireDurationSec = 600; // how long the summoned campfire (and NPCs) stay up
        uint32 checkIntervalMs = 5000;    // how often each player's proximity is re-checked
        bool vendorEnabled = false;
        bool repairEnabled = false;
        uint32 vendorEntry = 0; // creature_template entry, operator-supplied (see README)
        uint32 repairEntry = 0; // creature_template entry, operator-supplied (see README)
    };

    Config& GetConfig();
}

#endif // MOD_CAMPING_H
