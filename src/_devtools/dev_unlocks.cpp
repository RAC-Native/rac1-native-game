// _devtools/dev_unlocks.cpp — the debug menu's Unlocks tab (openly invented, ADR 0003; not a port).
//
// Everything here writes the game's own state, as the game's code writes it (the EE addresses of the ported code):
// - the items owned 0x13d4c0[0x25] and the "seen" bytes 0x13d4e8[0x25]: given by the game's own item_give (0x260860, through
//   item_give_quiet), taken back by clearing what it set (owned, seen) and the places the item sits (the quick select's ring
//   0x141ea0[8], the level gadgets' list 0x15edd0[12], the worn slot: the request 0x141408[kind] = 0x26 "none", what the pause
//   menu's closing 0x276908 writes to take an item off; an item in hand: the wrench asked for, as the quick select does);
// - the goodies 0x13d388[i] (the pause menu's Items page, its cells of kind 1): Zoomerator 0, Raritanium 1, CodeBot 2,
//   Premium Nanotech 4, Ultra Nanotech 5; the nanotech also the health maximum 0x15eda0 (5 / 8; 4 without them) and the
//   health P+0x22a8 filled, as Orxon's vendor of the Ultra Nanotech and the challenge mode's new game (0x284950) do;
// - the gold bolts 0x14bec0[level * 4 + k], one byte a bolt (the gold bolt moby 1134 sets its pvar +0's byte when picked up);
//   the pause menu's statistics count them (0x264588, at most 40), less 4 per gold weapon (0x2645f8);
// - the gold weapons 0x13e520[item] (Novalis's gold weapon cases 304, L1:0x2e1ac0, set the weapon's byte when it is bought):
//   the menus' icons (+4 frame) and gold names, the quick select, the Vendor's models, the Help pages and the weapon in hand
//   (its gold colouring every frame, 0x207330; the GS registers 0x245770 adds when it loads the class) read it.
// The planets' gold bolt counts and the ten gold weapons are the game's (counted on every level: the gold bolt mobys'
// pvar +0; Novalis's table L1:0x1deb10); the item ids are the pause menu pages' cells (Gadgets 0x1b28e8, Items 0x1b5cb8).

#include "dev_unlocks.h"
#include "item_give_quiet.h"

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/ee_memory.h>

#include <imgui.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace rac1::devtools {

namespace {
constexpr uint32_t P = 0x0013f350;                  // the player
constexpr uint32_t HEALTH = P + 0x22a8;             // nanotech (health)
constexpr uint32_t HEALTH_MAX = 0x0015eda0;
constexpr uint32_t IN_HAND = P + 0x10b8;            // the item in hand
constexpr uint32_t ITEM_REQUEST = P + 0x20b8;       // the item asked for in hand (what the quick select writes)
constexpr uint32_t OWNED = 0x0013d4c0;              // items owned [0x25]
constexpr uint32_t SEEN = 0x0013d4e8;               // items given once [0x25] (item_give sets both)
constexpr uint32_t GOODIES = 0x0013d388;            // the goodies (and story flags past them)
constexpr uint32_t GOLD_WEAPONS = 0x0013e520;       // gold weapons owned [0x25]
constexpr uint32_t GOLD_BOLTS = 0x0014bec0;         // gold bolts found [20 levels][4]
constexpr uint32_t QS_RING = 0x00141ea0;            // the quick select's ring [8] (0 empty)
constexpr uint32_t QS_LEVEL = 0x0015edd0;           // the level gadgets' quick list [12] (item | 0x40, 0xff free)
constexpr uint32_t WORN = 0x00141660;               // the item in each slot [4] (the item table's +8 kind)
constexpr uint32_t WORN_REQUEST = 0x00141408;       // the slot's request [4] (0x26: none)
constexpr OvConst<0x00179ac0u> ITEM_ROWS{};         // items (0x4c): +8 kind (the slot)
constexpr int WRENCH = 8;

struct Entry {
    int id;
    const char* name;
};
// the pause menu's Gadgets page (grids 0x1b2b80..: the held gadgets, the packs, the helmets, the boots)
constexpr Entry GADGETS[] = {
    {0x0c, "Swingshot"},      {0x16, "Hydrodisplacer"}, {0x1a, "Trespasser"}, {0x1b, "Metal Detector"}, {0x1f, "Hologuise"},
    {0x20, "Gadgetron PDA"},  {0x02, "Heli-Pack"},      {0x03, "Thruster-Pack"}, {0x04, "Hydro-Pack"},   {0x05, "Sonic Summoner"},
    {0x06, "O2 Mask"},        {0x07, "Pilot's Helmet"}, {0x1c, "Magneboots"},  {0x1d, "Grindboots"}};
// the pause menu's Items page (grid 0x1b5e60): its items of 0x13d4c0
constexpr Entry ITEMS[] = {{0x1e, "Hoverboard"}, {0x21, "Map-o-matic"}, {0x22, "Bolt Grabber"}, {0x23, "Persuader"}};
// ... and its goodies of 0x13d388
constexpr Entry GOODIE_LIST[] = {{0, "Zoomerator"}, {1, "Raritanium"}, {2, "CodeBot"}, {4, "Premium Nanotech"}, {5, "Ultra Nanotech"}};
constexpr int PREMIUM = 4, ULTRA = 5;
// Novalis's gold weapon cases, in their order (L1:0x1deb10)
constexpr Entry GOLD_LIST[] = {{0x13, "Tesla Claw"},  {0x0a, "Bomb Glove"},  {0x0b, "Devastator"},    {0x10, "Pyrocitor"},
                               {0x11, "Mine Glove"},  {0x0f, "Blaster"},     {0x15, "Morph-o-ray"},   {0x14, "Glove of Doom"},
                               {0x19, "Decoy Glove"}, {0x09, "Suck Cannon"}};
// the gold bolts of each level (0..18): its bolts are k = 0..n-1 of 0x14bec0[level * 4 + k] (40 in all)
constexpr int GOLD_BOLTS_OF[19] = {0, 3, 4, 3, 1, 2, 2, 2, 2, 2, 2, 1, 2, 1, 4, 2, 2, 2, 3};
const char* const PLANET[19] = {"Veldin (tutorial)", "Novalis", "Aridia", "Kerwan", "Eudora", "Rilgar", "Nebula G34",
                                "Umbris", "Batalia", "Gaspar", "Orxon", "Pokitaru", "Hoven", "Oltanis Orbit", "Oltanis",
                                "Quartu", "Kalebo III", "Veldin Orbit", "Veldin (finale)"};

uint8_t rb(uint32_t a) { return ee_read<uint8_t>(a); }
void wb(uint32_t a, uint8_t v) { ee_write<uint8_t>(a, v); }
int32_t r32(uint32_t a) { return ee_read<int32_t>(a); }
void w32(uint32_t a, int32_t v) { ee_write<int32_t>(a, v); }

// --- the items of 0x13d4c0 ---
bool item_owned(int id) { return rb(OWNED + static_cast<uint32_t>(id)) != 0; }
void item_give(int id) {
    if (!item_owned(id)) item_give_quiet(id, 0);  // the game's own give (its message only when the level has its text)
}
void item_take(int id) {
    if (!item_owned(id)) return;
    const uint32_t i = static_cast<uint32_t>(id);
    wb(OWNED + i, 0);
    wb(SEEN + i, 0);
    for (uint32_t k = 0; k < 8; ++k)
        if (r32(QS_RING + k * 4) == id) w32(QS_RING + k * 4, 0);
    for (uint32_t k = 0; k < 12; ++k) {
        const uint8_t b = rb(QS_LEVEL + k);
        if (b != 0xff && (b & 0x3f) == id) wb(QS_LEVEL + k, 0xff);
    }
    const int32_t kind = ee_read<int32_t>(ITEM_ROWS + i * 0x4c + 8);
    if (kind == 0) {  // held: the wrench asked for in its place
        if (r32(IN_HAND) == id || r32(ITEM_REQUEST) == id || r32(WORN) == id) w32(ITEM_REQUEST, WRENCH);
    } else if (kind > 0 && kind < 4 && r32(WORN + static_cast<uint32_t>(kind) * 4) == id) {
        w32(WORN_REQUEST + static_cast<uint32_t>(kind) * 4, 0x26);  // taken off, as the pause menu does
    }
}

// --- the goodies of 0x13d388 ---
bool goodie_owned(int i) { return rb(GOODIES + static_cast<uint32_t>(i)) != 0; }
// the health maximum the nanotech goodies give (4 without them), the health kept within it
void nanotech_apply(bool fill) {
    const int32_t max = goodie_owned(ULTRA) ? 8 : goodie_owned(PREMIUM) ? 5 : 4;
    w32(HEALTH_MAX, max);
    if (fill || r32(HEALTH) > max) w32(HEALTH, max);
}
void goodie_set(int i, bool on) {
    if (goodie_owned(i) == on) return;
    wb(GOODIES + static_cast<uint32_t>(i), on ? 1 : 0);
    if (i == PREMIUM || i == ULTRA) nanotech_apply(on);
}

// --- gold bolts and gold weapons ---
uint32_t gold_bolt_addr(int level, int k) { return GOLD_BOLTS + static_cast<uint32_t>(level * 4 + k); }
void gold_bolts_set(int level, bool on) {
    for (int k = 0; k < GOLD_BOLTS_OF[level]; ++k) wb(gold_bolt_addr(level, k), on ? 1 : 0);
}
// the pause menu's statistics: found (0x264588), used (4 per gold weapon, 0x2645f8), left (0x264540)
int gold_bolts_found() {
    int n = 0;
    for (uint32_t i = 0; i < 0x50; ++i) n += rb(GOLD_BOLTS + i) != 0 ? 1 : 0;
    return n < 40 ? n : 40;
}
int gold_weapons_owned() {
    int n = 0;
    for (uint32_t i = 0; i < 0x25; ++i) n += rb(GOLD_WEAPONS + i) != 0 ? 1 : 0;
    return n < 10 ? n : 10;
}
bool gold_owned(int id) { return rb(GOLD_WEAPONS + static_cast<uint32_t>(id)) != 0; }
void gold_set(int id, bool on) { wb(GOLD_WEAPONS + static_cast<uint32_t>(id), on ? 1 : 0); }

// the "all" / "none" of each group
void items_all(bool on) {
    for (const Entry& e : GADGETS) on ? item_give(e.id) : item_take(e.id);
    for (const Entry& e : ITEMS) on ? item_give(e.id) : item_take(e.id);
    for (const Entry& e : GOODIE_LIST) goodie_set(e.id, on);
}
void bolts_all(bool on) {
    for (int l = 0; l < 19; ++l) gold_bolts_set(l, on);
}
void gold_all(bool on) {
    for (const Entry& e : GOLD_LIST) {
        if (on) item_give(e.id);  // (a gold weapon is sold only for a weapon owned)
        gold_set(e.id, on);
    }
}
bool apply_op(const std::string& op) {
    if (op == "items+" || op == "items-") items_all(op.back() == '+');
    else if (op == "bolts+" || op == "bolts-") bolts_all(op.back() == '+');
    else if (op == "gold+" || op == "gold-") gold_all(op.back() == '+');
    else if (op == "all+" || op == "all-") {
        items_all(op.back() == '+');
        bolts_all(op.back() == '+');
        gold_all(op.back() == '+');
    } else return false;
    return true;
}

// a grey explanation, wrapped to the window
void note(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// a group's "all" / "none" buttons
void all_none(const char* id, void (*set)(bool)) {
    ImGui::PushID(id);
    if (ImGui::SmallButton("All")) set(true);
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) set(false);
    ImGui::PopID();
}

void section_items() {
    note("The pause menu's Gadgets and Items pages. Given by the game's item_give (owned, its ammo / quick-select place); taken "
         "back: owned and seen cleared, out of the quick select, taken off (or the wrench in hand).");
    all_none("items", items_all);
    auto row = [](const Entry& e) {
        bool on = item_owned(e.id);
        char label[64];
        std::snprintf(label, sizeof label, "%s##i%d", e.name, e.id);
        if (ImGui::Checkbox(label, &on)) on ? item_give(e.id) : item_take(e.id);
    };
    if (ImGui::BeginTable("unlock_items", 3)) {
        for (const Entry& e : GADGETS) {
            ImGui::TableNextColumn();
            row(e);
        }
        for (const Entry& e : ITEMS) {
            ImGui::TableNextColumn();
            row(e);
        }
        for (const Entry& e : GOODIE_LIST) {
            ImGui::TableNextColumn();
            bool on = goodie_owned(e.id);
            char label[64];
            std::snprintf(label, sizeof label, "%s##g%d", e.name, e.id);
            if (ImGui::Checkbox(label, &on)) goodie_set(e.id, on);
        }
        ImGui::EndTable();
    }
    char line[128];
    std::snprintf(line, sizeof line, "Goodies: 0x13d388 (Premium / Ultra Nanotech: the health maximum 5 / 8, 4 without them; now %d).", r32(HEALTH_MAX));
    note(line);
}

void section_gold_bolts() {
    const int found = gold_bolts_found(), used = gold_weapons_owned() * 4;
    ImGui::Text("Found %d / 40   used %d (4 per gold weapon)   left %d", found, used, found - used < 0 ? 0 : found - used);
    note("0x14bec0[level * 4 + k]; the pause menu's Items page counts them. The bolts of the planet you are on show / hide "
         "when the planet is loaded again.");
    all_none("bolts", bolts_all);
    const int32_t cur = r32(0x0015ed84);
    if (ImGui::BeginTable("unlock_bolts", 3, ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Planet", ImGuiTableColumnFlags_WidthFixed, 170);
        ImGui::TableSetupColumn("Gold bolts", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("");
        for (int l = 0; l < 19; ++l) {
            if (GOLD_BOLTS_OF[l] == 0) continue;
            ImGui::PushID(l);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (l == cur) ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%2d %s", l, PLANET[l]);
            else ImGui::Text("%2d %s", l, PLANET[l]);
            ImGui::TableNextColumn();
            for (int k = 0; k < GOLD_BOLTS_OF[l]; ++k) {
                bool on = rb(gold_bolt_addr(l, k)) != 0;
                ImGui::PushID(k);
                if (ImGui::Checkbox("##b", &on)) wb(gold_bolt_addr(l, k), on ? 1 : 0);
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("All")) gold_bolts_set(l, true);
            ImGui::SameLine();
            if (ImGui::SmallButton("None")) gold_bolts_set(l, false);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void section_gold_weapons() {
    note("0x13e520[item], as Novalis's gold weapon cases set it: the menus' gold icons and names, the Vendor, the weapon's "
         "colours (the weapon in hand at once). All: the weapons given too (the cases sell a gold "
         "weapon only for one owned); None leaves the weapons.");
    all_none("gold", gold_all);
    if (ImGui::BeginTable("unlock_gold", 3)) {
        for (const Entry& e : GOLD_LIST) {
            ImGui::TableNextColumn();
            bool on = gold_owned(e.id);
            char label[64];
            std::snprintf(label, sizeof label, "Gold %s%s##w%d", e.name, item_owned(e.id) ? "" : " (not owned)", e.id);
            if (ImGui::Checkbox(label, &on)) gold_set(e.id, on);
        }
        ImGui::EndTable();
    }
}
}  // namespace

void tab_unlocks() {
    ImGui::TextWrapped("What the game keeps of what was found and bought, set through its own state (the same bytes and "
                       "writes as the game's code). Saved with the game like the rest.");
    if (ImGui::Button("Give everything")) apply_op("all+");
    ImGui::SameLine();
    if (ImGui::Button("Take everything back")) apply_op("all-");
    if (ImGui::CollapsingHeader("Items: gadgets, packs, helmets, boots, goodies", ImGuiTreeNodeFlags_DefaultOpen)) section_items();
    if (ImGui::CollapsingHeader("Gold bolts", ImGuiTreeNodeFlags_DefaultOpen)) section_gold_bolts();
    if (ImGui::CollapsingHeader("Gold weapons", ImGuiTreeNodeFlags_DefaultOpen)) section_gold_weapons();
}

void unlocks_testing(uint64_t frame) {
    static const char* spec = std::getenv("RAC1_UNLOCKS");
    if (spec == nullptr) return;
    for (const char* q = spec; *q != 0;) {
        char* e = nullptr;
        const uint64_t f = std::strtoull(q, &e, 10);
        if (e == q || *e != ':') break;
        const char* op = e + 1;
        const char* end = std::strchr(op, ',');
        const std::string s(op, end != nullptr ? static_cast<size_t>(end - op) : std::strlen(op));
        if (f == frame) {
            const bool ok = apply_op(s);
            std::fprintf(stderr, "[orig] RAC1_UNLOCKS: %s at frame %llu%s\n", s.c_str(), static_cast<unsigned long long>(frame), ok ? "" : " (unknown)");
        }
        if (end == nullptr) break;
        q = end + 1;
    }
}

}  // namespace rac1::devtools
