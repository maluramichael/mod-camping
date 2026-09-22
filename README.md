# mod-camping

An [AzerothCore](https://www.azerothcore.org/) module (WotLK 3.3.5a) that adds a campfire rest
spot: set up camp anywhere and get a stacking-free rest buff as long as you stay near the fire.

## What it does

```
.camp
```

Summons a campfire at your feet (on a per-player cooldown). While you stay within range of any
active campfire, a **Camp Rest** buff is kept topped up to its full duration; step away and it
just counts down and expires on its own — no cleanup needed.

- **Campfire** — a real, existing "Basic Campfire" gameobject (the same one used elsewhere in
  the game's own content), so it looks and behaves like Blizzard's own campfires.
- **Camp Rest buff** — a small out-of-combat health/mana regeneration bonus (+10%/+10%),
  refreshed to `Camping.BuffDurationMin` while in range. It's meant as a lightweight,
  standalone alternative to class buffs, not a replacement for them — see "Deferred" below.
- **Any campfire counts**, not just your own — including other players' campfires, or any
  Basic Campfire already standing somewhere in the world. Convenient, but a future version
  could restrict this to the caster's own fire if that turns out to matter.
- **Optional vendor/repair NPCs** — off by default; see "Vendor / repair NPCs" below.

## Configuration

`conf/mod_camping.conf.dist`:

| Key                          | Default | Description                                              |
|-------------------------------|---------|----------------------------------------------------------|
| `Camping.Enable`               | `1`     | Master on/off switch                                      |
| `Camping.BuffDurationMin`      | `60`    | Minutes the Camp Rest buff is topped up to while in range |
| `Camping.SummonCooldownSec`    | `300`   | Per-player cooldown on `.camp`                             |
| `Camping.RestRangeYards`       | `15`    | Range to receive/keep the rest buff                        |
| `Camping.CampfireDurationSec`  | `600`   | How long the campfire (and NPCs) stay up                   |
| `Camping.CheckIntervalMs`      | `5000`  | How often proximity is re-checked per player                |
| `Camping.VendorEnabled`        | `0`     | Also summon a vendor NPC (needs `Camping.VendorEntry`)      |
| `Camping.VendorEntry`          | `0`     | creature_template entry for the vendor NPC                  |
| `Camping.RepairEnabled`        | `0`     | Also summon a repair NPC (needs `Camping.RepairEntry`)       |
| `Camping.RepairEntry`          | `0`     | creature_template entry for the repair NPC                  |

## Vendor / repair NPCs

The convenience NPCs are intentionally **not** hardcoded to a specific creature entry, because
that entry has to actually exist in *your* world database, and a wrong one silently does
nothing useful. Instead:

1. Find a vendor/repair NPC entry you're happy with in your own `creature_template` table.
2. Set `Camping.VendorEntry` / `Camping.RepairEntry` to that entry and flip the matching
   `...Enabled` key to `1`.
3. On worldserver startup the module checks both entries against the DB and logs an error (and
   quietly keeps that NPC off) if either one doesn't resolve to a real creature — it will never
   try to summon an invented ID.

## Deferred (out of scope for v1)

The original design also included a full profession-object / blueprint / inventory-slot system
(craftable camp upgrades, placeable gear, etc.). That's intentionally **not** part of this
release — v1 is the campfire + rest buff loop only. A future version could also:

- Restrict the rest buff to the caster's own campfire instead of any Basic Campfire.
- Make the buff's aura category / stacking rules interact explicitly with class buffs instead
  of being fully standalone.

## Installation

Clone into your AzerothCore `modules/` directory and rebuild the worldserver. The module's SQL
(the Camp Rest carrier spell) is applied automatically by the DB updater on the next world
start.

## License

Released under the GNU GPL v2 (or later).
