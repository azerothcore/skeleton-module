# mod-better-roll-play

### Better role-play interactions for AzerothCore 3.3.5a

**DE:** Dieses Modul verleiht NPCs mehr Leben: Begrüßungen mit Zufallstexten,
zusätzliche Emotes, optionale Follow‑ups, Reaktionen auf Spieler‑Emotes und
ambientes Verhalten ohne Datenbankabhängigkeit.

**EN:** This module adds light‑weight role‑play flavour to NPCs: random greeting
texts, extra emotes, optional follow‑ups, reactions to player emotes and idle
ambient emotes – all without any database requirements.

## Features

- Dynamic gossip greetings with optional random emotes
- Optional follow‑up phrases after greeting
- Locale support (deDE / enUS fallback)
- Optional ambient idle emotes when players are nearby
- NPC reaction to supported player text emotes
- Whitelist/Blacklist filters by Creature entry
- Pure configuration, no SQL or DB dependencies

## Installation

1. Place the `mod-better-roll-play` folder inside your AzerothCore `modules/`
   directory.
2. Copy `mod_better_roll_play.conf.dist` to your server's `etc/` directory and
   rename it to `mod_better_roll_play.conf` if needed.
3. Reconfigure CMake and rebuild the core.
4. Restart `worldserver`.

### Sample configuration

```conf
BetterRP.Enable = 1
BetterRP.Greeting.ExtraEmoteChance = 50
BetterRP.Texts.EN = "Hello, {name}!|Welcome, {name}!|Greetings, {name}!"
BetterRP.Texts.DE = "Hallo, {name}!|Willkommen, {name}!|Seid gegrüßt, {name}!"
```

## Configuration options

| Option | Description |
|-------|-------------|
| `BetterRP.Enable` | Master enable switch |
| `BetterRP.Greeting.CooldownMs` | Per player/creature greeting cooldown |
| `BetterRP.Greeting.UseExtraEmote` | Enable random extra emote on greeting |
| `BetterRP.Greeting.ExtraEmoteChance` | Chance for extra emote (0‑100) |
| `BetterRP.Greeting.ExtraEmotes` | Comma list of emotes for extra emote |
| `BetterRP.Greeting.YellChance` | Chance to yell instead of say |
| `BetterRP.Texts.EN` / `BetterRP.Texts.DE` | Pipe‑separated greeting texts |
| `BetterRP.Followup.Enable` | Enable optional follow‑up phrases |
| `BetterRP.Followup.Chance` | Chance to trigger follow‑up |
| `BetterRP.Followup.Texts.EN` / `.DE` | Pipe‑separated follow‑up texts |
| `BetterRP.Filter.Whitelist` | Optional CSV whitelist of creature entries |
| `BetterRP.Filter.Blacklist` | CSV blacklist of creature entries |
| `BetterRP.Ambient.Enable` | Enable ambient idle emotes |
| `BetterRP.Ambient.IntervalMs` | Idle emote interval when players nearby |
| `BetterRP.Ambient.RangeMin` / `.RangeMax` | Range check for ambient emotes |
| `BetterRP.Ambient.Emotes` | Comma list of ambient emotes |
| `BetterRP.ReactToPlayerEmotes` | Enable reaction to player text emotes |
| `BetterRP.React.CooldownMs` | Cooldown between reactions |
| `BetterRP.React.RangeMax` | Max distance to react to player emotes |
| `BetterRP.React.Supported` | Comma list of supported player emotes |

## Compatibility

Tested with `azerothcore/azerothcore-wotlk` (3.3.5a).

