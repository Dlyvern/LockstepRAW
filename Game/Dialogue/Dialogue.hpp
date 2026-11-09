#ifndef LOCKSTEP_GAME_DIALOGUE_DIALOGUE_HPP
#define LOCKSTEP_GAME_DIALOGUE_DIALOGUE_HPP

#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace lockstep::assets
{
    class FileSystem;
} //namespace lockstep::assets

namespace game
{
    // Speaker of lines that are on-screen text (objectives, tips) rather than spoken.
    inline constexpr std::string_view k_hudSpeaker{"hud"};

    // One line of a dialogue file.
    // id:      stable id, e.g. "station.chief.briefing.03".
    // speaker: who says it ("chief", "range_officer"), or k_hudSpeaker.
    // text:    what is said.
    struct DialogueLine
    {
        std::string id;
        std::string speaker;
        std::string text;
    };

    // The dialogue of one level, read from "dialogue/<level>.txt" in the mounted content (the
    // repository's Data/ folder is mounted by default). Presentation data: every machine loads
    // its own copy.
    // A file may pull in another one with a line `@include <name>` ("dialogue/<name>.txt"), so
    // levels can share lines (the apartment and the district both include "morning").
    // lines:    every line, sorted by id.
    // includes: names of the files this one includes, as parsed.
    // missing:  ids and prefixes that were asked for but not found; each is logged once.
    struct DialogueBook
    {
        std::vector<DialogueLine> lines;
        std::vector<std::string> includes;
        std::vector<std::string> missing;
    };

    // One subtitle waiting or on screen.
    // text:    what is shown ("CHIEF: ...").
    // seconds: Player seconds it stays on screen once shown.
    struct Subtitle
    {
        std::string text;
        float seconds{0.0f};
    };

    // The local player's subtitles: the one shown first, then the ones queued after it.
    // Client-only presentation, never replicated.
    struct Subtitles
    {
        std::deque<Subtitle> queue;
    };

    // level:   a level's virtual path ("levels/station/scene") or scene file path.
    // Returns: the level's short name: the folder after "levels/" ("station"), else the file
    //          name without extension.
    [[nodiscard]] std::string levelShortName(std::string_view level);

    // Parses a dialogue file: one `id | speaker | text` per line; blank lines and lines starting
    // with '#' are skipped; `@include <name>` lines are listed in `includes` (not loaded here);
    // malformed lines are logged and skipped.
    // text:    the file's contents.
    // source:  the file name, for the log.
    // Returns: the book, sorted by id.
    [[nodiscard]] DialogueBook parseDialogue(std::string_view text, std::string_view source);

    // Loads "dialogue/<level>.txt" and the files it includes through the file system and logs
    // how many lines it has.
    // files:   the mounted content.
    // level:   the level's short name (levelShortName()).
    // Returns: the book; empty (with a log line) when the file is missing.
    [[nodiscard]] DialogueBook loadDialogue(const lockstep::assets::FileSystem& files, std::string_view level);

    // Finds a line by id. A missing id is logged once.
    // book:    the dialogue.
    // id:      the line's id.
    // Returns: the line, or null.
    [[nodiscard]] const DialogueLine* findLine(DialogueBook& book, std::string_view id);

    // The text of an on-screen line (objective, narration, notice, caption) by id.
    // book:    the dialogue.
    // id:      the line's id.
    // Returns: the line's text, or the id itself when it is missing (logged once). Valid until
    //          the book changes.
    [[nodiscard]] std::string_view lineText(DialogueBook& book, std::string_view id);

    // Lists the lines whose id starts with a prefix. An empty result is logged once per prefix.
    // book:    the dialogue.
    // prefix:  e.g. "station.chief.briefing.".
    // Returns: the lines, in id order.
    [[nodiscard]] std::vector<const DialogueLine*> linesWithPrefix(DialogueBook& book, std::string_view prefix);

    // How a line reads as a subtitle.
    // line:    the line.
    // Returns: "SPEAKER: text" (speaker upper-cased, '_' as a space), or the text alone for
    //          k_hudSpeaker lines.
    [[nodiscard]] std::string subtitleText(const DialogueLine& line);

    // text:    a subtitle.
    // Returns: Player seconds it stays on screen, longer for longer text (2 to 8 s).
    [[nodiscard]] float subtitleSeconds(std::string_view text);

    // Queues the line with this id; a missing id queues the id itself (and is logged once).
    // subtitles: the local subtitles.
    // book:      the dialogue.
    // id:        the line's id.
    void queueLine(Subtitles& subtitles, DialogueBook& book, std::string_view id);

    // Queues every line whose id starts with `prefix`, in id order; without any, queues
    // `fallbackId` as text so the gap is visible.
    // subtitles:  the local subtitles.
    // book:       the dialogue.
    // prefix:     the id prefix, e.g. "station.chief.briefing.".
    // fallbackId: shown when no line has the prefix.
    void queueLines(Subtitles& subtitles, DialogueBook& book, std::string_view prefix, std::string_view fallbackId);

    // book:       the dialogue.
    // prefix:     an id prefix.
    // fallbackId: used when no line has the prefix.
    // Returns:    Player seconds queueLines() with the same arguments keeps subtitles on screen.
    [[nodiscard]] float linesSeconds(DialogueBook& book, std::string_view prefix, std::string_view fallbackId);

    // subtitles: the local subtitles.
    // Returns:   true while a subtitle is on screen.
    [[nodiscard]] bool isSpeaking(const Subtitles& subtitles);

    // Ages the subtitle on screen by one tick; the next one follows when it runs out.
    // subtitles:   the local subtitles.
    // playerDelta: Player domain delta of this tick.
    void stepSubtitles(Subtitles& subtitles, float playerDelta);

    // subtitles: the local subtitles.
    // Returns:   the subtitle on screen, empty for none.
    [[nodiscard]] std::string_view currentSubtitle(const Subtitles& subtitles);
} //namespace game

#endif //LOCKSTEP_GAME_DIALOGUE_DIALOGUE_HPP
