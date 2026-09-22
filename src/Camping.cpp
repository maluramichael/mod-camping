/*
 * mod-camping
 *
 * A campfire rest spot, v1 (campfire + rest buff loop only - see README.md "Deferred" for the
 * rest of the original design: profession objects, blueprints, inventory slots).
 *
 *   1. `.camp` summons an existing "Basic Campfire" gameobject (entry 29784, Blizzard's own
 *      WotLK content - see Camping::GO_BASIC_CAMPFIRE in Camping.h for how it was verified) at
 *      the caster's feet, on a per-player cooldown. Optionally also summons a vendor and/or a
 *      repair NPC nearby (config-gated, off by default - see README "Vendor / repair NPCs").
 *   2. While a player is within range of ANY active Basic Campfire (their own summon, someone
 *      else's, or one Blizzard already placed in the world), a "Camp Rest" aura (server-side
 *      spell 200240, created by this module's SQL - same pattern as mod-group-buffs) is kept
 *      refreshed to a full BuffDurationMin. Leaving range simply lets the timer already on the
 *      aura count down and expire naturally - no bookkeeping needed.
 *
 * Proximity uses WorldObject::FindNearestGameObject(), so no registry of spawned campfires is
 * kept; it is checked on a light per-player timer (Config::checkIntervalMs) in
 * PlayerScript::OnPlayerUpdate, same shape as mod-group-buffs' periodic re-evaluation.
 *
 * Released under GNU GPL v2; redistribute/modify under version 2 of the License, or (at your
 * option) any later version.
 */

#include "Camping.h"

#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "GameObject.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Timer.h"
#include <algorithm>

namespace Camping
{
    Config& GetConfig()
    {
        static Config cfg;
        return cfg;
    }
}

using namespace Acore::ChatCommands;
using namespace Camping;

namespace
{
    // False when SPELL_CAMP_REST is missing from spell_dbc (module SQL not applied): `.camp`
    // still works (the campfire itself is just a gameobject), only the rest buff is skipped.
    bool sRestSpellAvailable = true;

    // Per-player state, lives in Player::CustomData (each player is only ever touched by the
    // world-update thread owning its map, same assumption mod-group-buffs relies on).
    struct CampingData : public DataMap::Base
    {
        uint32 lastSummonMs = 0;   // getMSTime() of the last `.camp` use (0 = never used)
        uint32 checkElapsedMs = 0; // time since the last proximity check
    };

    std::string const DATA_KEY = "Camping";

    // Modest out-of-combat regeneration bump while camped - the "alternative to class buffs"
    // v1 placeholder mentioned in the README; amounts are deliberately small and independent of
    // (never stack with) whatever else is boosting regen.
    constexpr int32 CAMP_REST_HEALTH_REGEN_PCT = 10;
    constexpr int32 CAMP_REST_POWER_REGEN_PCT = 10;

    // Applies (creating on demand) the Camp Rest aura at full duration. Idempotent.
    void RefreshCampRestAura(Player* player)
    {
        Aura* aura = player->GetAura(SPELL_CAMP_REST);
        if (!aura)
        {
            aura = player->AddAura(SPELL_CAMP_REST, player);
            if (!aura)
                return;
        }

        if (AuraEffect* regenHealth = aura->GetEffect(0))
            if (regenHealth->GetAmount() != CAMP_REST_HEALTH_REGEN_PCT)
                regenHealth->ChangeAmount(CAMP_REST_HEALTH_REGEN_PCT);

        if (AuraEffect* regenPower = aura->GetEffect(1))
            if (regenPower->GetAmount() != CAMP_REST_POWER_REGEN_PCT)
                regenPower->ChangeAmount(CAMP_REST_POWER_REGEN_PCT);

        int32 const durationMs = static_cast<int32>(GetConfig().buffDurationMin * MINUTE * IN_MILLISECONDS);
        aura->SetMaxDuration(durationMs);
        aura->SetDuration(durationMs);
    }

    Position NearOffset(Player* player, float distanceYards, float angleOffset)
    {
        return player->GetNearPosition(distanceYards, angleOffset);
    }
}

// =====================================================================
//  CommandScript: `.camp`
// =====================================================================
class camping_commandscript : public CommandScript
{
public:
    camping_commandscript() : CommandScript("camping_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "camp", HandleCampCommand, SEC_PLAYER, Console::No },
        };

        return commandTable;
    }

    static bool HandleCampCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Config const& cfg = GetConfig();
        if (!cfg.enable)
        {
            handler->SendSysMessage("Camping is currently disabled on this server.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        CampingData* data = player->CustomData.GetDefault<CampingData>(DATA_KEY);
        uint32 const now = getMSTime();
        uint32 const cooldownMs = cfg.summonCooldownSec * IN_MILLISECONDS;

        if (data->lastSummonMs != 0 && getMSTimeDiff(data->lastSummonMs, now) < cooldownMs)
        {
            uint32 const remainingSec =
                (cooldownMs - getMSTimeDiff(data->lastSummonMs, now)) / IN_MILLISECONDS;
            handler->PSendSysMessage("You must wait {} more second(s) before setting up another camp.", remainingSec);
            handler->SetSentErrorMessage(true);
            return false;
        }

        GameObject* fire = player->SummonGameObject(GO_BASIC_CAMPFIRE,
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(),
            0.0f, 0.0f, 0.0f, 0.0f, cfg.campfireDurationSec);

        if (!fire)
        {
            handler->SendSysMessage(
                "|cffff0000[Camping]|r Could not summon the campfire (gameobject template missing?).");
            handler->SetSentErrorMessage(true);
            return false;
        }

        data->lastSummonMs = now;

        if (sRestSpellAvailable)
            handler->PSendSysMessage(
                "|cff4CFF00[Camping]|r Camp set up. Stay within {} yards to keep your Camp Rest buff topped up "
                "(up to {} minute(s)).", cfg.restRangeYards, cfg.buffDurationMin);
        else
            handler->SendSysMessage(
                "|cff4CFF00[Camping]|r Camp set up (rest buff unavailable - server-side spell not installed).");

        if (cfg.vendorEnabled && cfg.vendorEntry != 0)
        {
            Position const pos = NearOffset(player, 4.0f, 1.5708f);
            player->SummonCreature(cfg.vendorEntry, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
                pos.GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, cfg.campfireDurationSec * IN_MILLISECONDS);
        }

        if (cfg.repairEnabled && cfg.repairEntry != 0)
        {
            Position const pos = NearOffset(player, 4.0f, -1.5708f);
            player->SummonCreature(cfg.repairEntry, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
                pos.GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, cfg.campfireDurationSec * IN_MILLISECONDS);
        }

        return true;
    }
};

// =====================================================================
//  PlayerScript: periodic proximity check -> keeps the Camp Rest aura topped up.
// =====================================================================
class CampingPlayerScript : public PlayerScript
{
public:
    CampingPlayerScript() : PlayerScript("CampingPlayerScript", { PLAYERHOOK_ON_UPDATE }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        Config const& cfg = GetConfig();
        if (!cfg.enable || !sRestSpellAvailable)
            return;

        CampingData* data = player->CustomData.GetDefault<CampingData>(DATA_KEY);
        data->checkElapsedMs += diff;
        if (data->checkElapsedMs < cfg.checkIntervalMs)
            return;

        data->checkElapsedMs = 0;

        if (!player->IsInWorld())
            return;

        // Any Basic Campfire counts - a self-summoned one, someone else's, or content Blizzard
        // already placed in the world. See README "Any campfire, not just your own".
        if (player->FindNearestGameObject(GO_BASIC_CAMPFIRE, cfg.restRangeYards, true))
            RefreshCampRestAura(player);
    }
};

// =====================================================================
//  WorldScript: config load + one-time sanity checks against the DB stores.
// =====================================================================
class CampingWorldScript : public WorldScript
{
public:
    CampingWorldScript() : WorldScript("CampingWorldScript",
        { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        Config& cfg = GetConfig();
        cfg.enable = sConfigMgr->GetOption<bool>("Camping.Enable", true);
        cfg.buffDurationMin = std::max<uint32>(sConfigMgr->GetOption<uint32>("Camping.BuffDurationMin", 60), 1);
        cfg.summonCooldownSec = sConfigMgr->GetOption<uint32>("Camping.SummonCooldownSec", 300);
        cfg.restRangeYards = std::max(1.0f, sConfigMgr->GetOption<float>("Camping.RestRangeYards", 15.0f));
        cfg.campfireDurationSec =
            std::max<uint32>(sConfigMgr->GetOption<uint32>("Camping.CampfireDurationSec", 600), 30);
        cfg.checkIntervalMs = std::max<uint32>(sConfigMgr->GetOption<uint32>("Camping.CheckIntervalMs", 5000), 500);
        cfg.vendorEnabled = sConfigMgr->GetOption<bool>("Camping.VendorEnabled", false);
        cfg.repairEnabled = sConfigMgr->GetOption<bool>("Camping.RepairEnabled", false);
        cfg.vendorEntry = sConfigMgr->GetOption<uint32>("Camping.VendorEntry", 0);
        cfg.repairEntry = sConfigMgr->GetOption<uint32>("Camping.RepairEntry", 0);
    }

    // Stores are loaded by now: verify the campfire gameobject template and the carrier spell
    // both actually exist, and quietly disable what is missing instead of crashing later.
    void OnStartup() override
    {
        Config& cfg = GetConfig();

        if (!sObjectMgr->GetGameObjectTemplate(GO_BASIC_CAMPFIRE))
        {
            LOG_ERROR("server.loading",
                "mod-camping: gameobject_template entry {} (Basic Campfire) is missing from the world DB. "
                "`.camp` will fail to summon a campfire until this is fixed.", GO_BASIC_CAMPFIRE);
        }

        sRestSpellAvailable = sSpellMgr->GetSpellInfo(SPELL_CAMP_REST) != nullptr;
        if (!sRestSpellAvailable)
        {
            LOG_ERROR("server.loading",
                "mod-camping: server-side spell {} is missing from spell_dbc "
                "(data/sql/db-world/updates/mod_camping_*.sql not applied?). The Camp Rest buff is disabled, "
                "`.camp` still summons the campfire itself.", SPELL_CAMP_REST);
        }

        if (cfg.vendorEnabled)
        {
            if (cfg.vendorEntry == 0 || !sObjectMgr->GetCreatureTemplate(cfg.vendorEntry))
            {
                LOG_ERROR("server.loading",
                    "mod-camping: Camping.VendorEnabled is 1 but Camping.VendorEntry ({}) is not a valid "
                    "creature_template entry - the vendor NPC will not be summoned. Set a verified entry from "
                    "your world DB (see README).", cfg.vendorEntry);
                cfg.vendorEnabled = false;
            }
        }

        if (cfg.repairEnabled)
        {
            if (cfg.repairEntry == 0 || !sObjectMgr->GetCreatureTemplate(cfg.repairEntry))
            {
                LOG_ERROR("server.loading",
                    "mod-camping: Camping.RepairEnabled is 1 but Camping.RepairEntry ({}) is not a valid "
                    "creature_template entry - the repair NPC will not be summoned. Set a verified entry from "
                    "your world DB (see README).", cfg.repairEntry);
                cfg.repairEnabled = false;
            }
        }

        LOG_INFO("server.loading", "mod-camping: loaded (enabled: {}, rest buff available: {}).",
            cfg.enable, sRestSpellAvailable);
    }
};

void AddCampingScripts()
{
    new camping_commandscript();
    new CampingPlayerScript();
    new CampingWorldScript();
}
