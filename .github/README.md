mod-better-roll-play
English | Deutsch

Overview
mod-better-roll-play is an AzerothCore module for 3.3.5a that makes NPC interactions feel more alive — completely without any database dependencies.
It enhances the roleplay atmosphere with configurable greetings, random emotes, optional follow-up dialogues, ambient idle emotes, and reactions to player emotes.
All texts and emotes are controlled via a .conf file.

Features
Gossip Greeting:

NPC waves (EMOTE_ONESHOT_WAVE) when opening the gossip menu.

Sends a random greeting text in the client’s language (DE/EN).

Optional extra emotes (e.g., BOW, DANCE, CHEER).

Optional follow-up phrases after the greeting.

Configurable yell chance.

Ambient Idle Emotes:

NPCs can perform random emotes at certain intervals when players are nearby.

Configurable emote list, range, and interval.

Reaction to Player Emotes:

Detects supported player text emotes nearby (e.g., WAVE, DANCE, BOW).

NPC mirrors the emote with a short delay.

Whitelist/Blacklist:

Only specific NPCs react (whitelist) or specific NPCs are excluded (blacklist).

Configurable via .conf:

All texts, emotes, probabilities, distances, and cooldowns.

No database changes required.

Installation
Place the module folder into your AzerothCore source directory:

bash
Kopieren
Bearbeiten
modules/mod-better-roll-play/
Make sure the following files are present:

CMakeLists.txt

mod_better_roll_play.cpp

mod_better_roll_play.conf.dist

Rebuild the core:

bash
Kopieren
Bearbeiten
cmake ../ -DCMAKE_BUILD_TYPE=Release
make
make install
Copy mod_better_roll_play.conf.dist into your etc/ folder and rename it to:

Kopieren
Bearbeiten
mod_better_roll_play.conf
Adjust the settings as desired.

Configuration Example
(from mod_better_roll_play.conf.dist)

ini
Kopieren
Bearbeiten
BetterRP.Enable = 1
BetterRP.Greeting.CooldownMs = 5000
BetterRP.Greeting.ExtraEmoteChance = 50
BetterRP.Texts.EN = "Hello, {name}!|Welcome, {name}!"
BetterRP.Texts.DE = "Hallo, {name}!|Willkommen, {name}!"
BetterRP.Ambient.Enable = 0
BetterRP.ReactToPlayerEmotes = 1
Compatibility
Fully compatible with azerothcore/azerothcore-wotlk

No changes to core or database required.

License
This module is released under the MIT License.
You are free to use and modify it privately. Improvements to the core must be contributed back to the main project under the AGPL license.
