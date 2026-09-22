-- mod-camping: "Camp Rest" server-side carrier spell.
--
-- The module applies it to a player as a self-aura (Unit::AddAura) and rewrites the effect
-- amounts and duration at runtime (AuraEffect::ChangeAmount / Aura::SetDuration), same pattern
-- as mod-group-buffs' carrier spells:
--   * EquippedItemClass = -1 -> no weapon/item requirement.
--   * DurationIndex 21 = infinite in the DBC; the module overrides the real duration in code
--     each time the buff is refreshed, so it still counts down and expires normally on the
--     client once the player leaves campfire range for good.
--   * CastingTimeIndex 1 = instant, RangeIndex 1 = self.
--   * Effect_1 = 6 (SPELL_EFFECT_APPLY_AURA), EffectAura_1 = 88 (MOD_HEALTH_REGEN_PERCENT)
--     Effect_2 = 6 (SPELL_EFFECT_APPLY_AURA), EffectAura_2 = 110 (MOD_POWER_REGEN_PERCENT, mana)
--     Base amounts are 0 in the DBC; the module sets both to +10% via ChangeAmount.
--
-- Deliberately NOT hidden (no PASSIVE / DO_NOT_DISPLAY attributes, unlike mod-group-buffs'
-- carrier spells): this one is meant to be a visible "Camp Rest" buff icon with a real
-- countdown timer. Attributes = NO_IMMUNITIES (0x20000000) only, so nothing accidentally
-- blocks it from applying.
--
-- Idempotent (DELETE + INSERT), only touches spell id 200240.

DELETE FROM `spell_dbc` WHERE `ID` = 200240;

INSERT INTO `spell_dbc`
(`ID`, `Attributes`, `EquippedItemClass`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, `SchoolMask`,
 `Effect_1`, `Effect_2`,
 `ImplicitTargetA_1`, `ImplicitTargetA_2`,
 `EffectAura_1`, `EffectAura_2`,
 `EffectMiscValue_1`, `EffectMiscValue_2`,
 `Name_Lang_enUS`, `Name_Lang_Mask`)
VALUES
(200240, 536870912, -1, 1, 21, 1, 1, 6, 6, 1, 1, 88, 110, 0, 0, 'Camp Rest', 16712190);
