#include "Dialogue/Dialogue.hpp"

#include <Lockstep/Assets/FileSystem.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>

namespace game
{
    namespace
    {
        constexpr std::string_view k_levelsFolder{"levels/"};
        constexpr std::string_view k_dialogueFolder{"dialogue/"};
        constexpr std::string_view k_dialogueExtension{".txt"};
        constexpr std::string_view k_whitespace{" \t\r"};
        constexpr char k_separator{'|'};
        constexpr char k_comment{'#'};
        constexpr std::string_view k_includeDirective{"@include"};
        constexpr int k_maxIncludeDepth{4};
        constexpr float k_baseSeconds{1.2f};
        constexpr float k_secondsPerCharacter{0.055f};
        constexpr float k_minSeconds{2.0f};
        constexpr float k_maxSeconds{8.0f};
        constexpr std::size_t k_wrapColumn{70};

        std::string_view trim(std::string_view text)
        {
            const std::size_t first = text.find_first_not_of(k_whitespace);

            if (first == std::string_view::npos)
            {
                return {};
            }

            const std::size_t last = text.find_last_not_of(k_whitespace);

            return text.substr(first, last - first + 1);
        }

        bool logMissingOnce(DialogueBook& book, std::string_view what, std::string_view kind)
        {
            if (std::find(book.missing.begin(), book.missing.end(), what) != book.missing.end())
            {
                return false;
            }

            book.missing.emplace_back(what);
            std::clog << "Game: dialogue: missing " << kind << " '" << what << "'\n";

            return true;
        }

        std::string wrap(std::string text)
        {
            std::size_t lineStart = 0;

            while (text.size() - lineStart > k_wrapColumn)
            {
                const std::size_t space = text.rfind(' ', lineStart + k_wrapColumn);

                if (space == std::string::npos || space <= lineStart)
                {
                    break;
                }

                text[space] = '\n';
                lineStart = space + 1;
            }

            return text;
        }

        void sortLines(DialogueBook& book)
        {
            std::stable_sort(book.lines.begin(), book.lines.end(),
                [](const DialogueLine& left, const DialogueLine& right) { return left.id < right.id; });
        }

        DialogueBook loadDialogueFile(const lockstep::assets::FileSystem& files, std::string_view name, int depth)
        {
            const std::string path = std::string(k_dialogueFolder) + std::string(name) + std::string(k_dialogueExtension);

            if (!files.exists(path))
            {
                std::clog << "Game: dialogue: no '" << path << "' in the mounted content; lines show their ids\n";
                return {};
            }

            auto bytes = files.read(path);

            if (!bytes)
            {
                std::clog << "Game: dialogue: '" << path << "' not read: " << bytes.error().message << '\n';
                return {};
            }

            const std::string_view text(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size());
            DialogueBook book = parseDialogue(text, path);

            if (depth >= k_maxIncludeDepth && !book.includes.empty())
            {
                std::clog << "Game: dialogue: '" << path << "' includes too deeply; its includes are skipped\n";
                return book;
            }

            for (const std::string& include : book.includes)
            {
                DialogueBook included = loadDialogueFile(files, include, depth + 1);
                std::move(included.lines.begin(), included.lines.end(), std::back_inserter(book.lines));
            }

            sortLines(book);

            return book;
        }

        void pushSubtitle(Subtitles& subtitles, std::string text)
        {
            const float seconds = subtitleSeconds(text);
            subtitles.queue.push_back(Subtitle{.text = wrap(std::move(text)), .seconds = seconds});
        }
    } //namespace

    std::string levelShortName(std::string_view level)
    {
        if (const std::size_t start = level.find(k_levelsFolder); start != std::string_view::npos)
        {
            const std::string_view rest = level.substr(start + k_levelsFolder.size());
            return std::string(rest.substr(0, rest.find('/')));
        }

        const std::size_t slash = level.find_last_of("/\\");
        const std::string_view file = slash == std::string_view::npos ? level : level.substr(slash + 1);

        return std::string(file.substr(0, file.find('.')));
    }

    DialogueBook parseDialogue(std::string_view text, std::string_view source)
    {
        DialogueBook book{};
        std::size_t lineNumber = 0;

        while (!text.empty())
        {
            const std::size_t end = text.find('\n');
            const std::string_view raw = text.substr(0, end);
            text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
            ++lineNumber;
            const std::string_view line = trim(raw);

            if (line.empty() || line.front() == k_comment)
            {
                continue;
            }

            if (line.starts_with(k_includeDirective))
            {
                const std::string_view name = trim(line.substr(k_includeDirective.size()));

                if (name.empty())
                {
                    std::clog << "Game: dialogue: " << source << ":" << lineNumber << " includes no file\n";
                    continue;
                }

                book.includes.emplace_back(name);
                continue;
            }

            const std::size_t first = line.find(k_separator);
            const std::size_t second = first == std::string_view::npos ? first : line.find(k_separator, first + 1);

            if (second == std::string_view::npos)
            {
                std::clog << "Game: dialogue: " << source << ":" << lineNumber << " is not 'id | speaker | text'\n";
                continue;
            }

            DialogueLine parsed{.id = std::string(trim(line.substr(0, first))),
                .speaker = std::string(trim(line.substr(first + 1, second - first - 1))),
                .text = std::string(trim(line.substr(second + 1)))};

            if (parsed.id.empty() || parsed.text.empty())
            {
                std::clog << "Game: dialogue: " << source << ":" << lineNumber << " has no id or no text\n";
                continue;
            }

            book.lines.push_back(std::move(parsed));
        }

        sortLines(book);

        return book;
    }

    DialogueBook loadDialogue(const lockstep::assets::FileSystem& files, std::string_view level)
    {
        DialogueBook book = loadDialogueFile(files, level, 0);
        std::clog << "Game: dialogue: " << book.lines.size() << " lines for level '" << level << "'\n";

        return book;
    }

    const DialogueLine* findLine(DialogueBook& book, std::string_view id)
    {
        const auto found = std::lower_bound(book.lines.begin(), book.lines.end(), id,
            [](const DialogueLine& line, std::string_view wanted) { return line.id < wanted; });

        if (found != book.lines.end() && found->id == id)
        {
            return &*found;
        }

        logMissingOnce(book, id, "line");

        return nullptr;
    }

    std::string_view lineText(DialogueBook& book, std::string_view id)
    {
        const DialogueLine* line = findLine(book, id);

        return line != nullptr ? std::string_view(line->text) : id;
    }

    std::vector<const DialogueLine*> linesWithPrefix(DialogueBook& book, std::string_view prefix)
    {
        std::vector<const DialogueLine*> lines;

        for (const DialogueLine& line : book.lines)
        {
            if (std::string_view(line.id).starts_with(prefix))
            {
                lines.push_back(&line);
            }
        }

        if (lines.empty())
        {
            logMissingOnce(book, prefix, "lines starting with");
        }

        return lines;
    }

    std::string subtitleText(const DialogueLine& line)
    {
        if (line.speaker.empty() || line.speaker == k_hudSpeaker)
        {
            return line.text;
        }

        std::string speaker = line.speaker;

        for (char& character : speaker)
        {
            character = character == '_' ? ' ' : static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        }

        return speaker + ": " + line.text;
    }

    float subtitleSeconds(std::string_view text)
    {
        return std::clamp(k_baseSeconds + k_secondsPerCharacter * static_cast<float>(text.size()), k_minSeconds,
            k_maxSeconds);
    }

    void queueLine(Subtitles& subtitles, DialogueBook& book, std::string_view id)
    {
        const DialogueLine* line = findLine(book, id);
        pushSubtitle(subtitles, line != nullptr ? subtitleText(*line) : std::string(id));
    }

    void queueLines(Subtitles& subtitles, DialogueBook& book, std::string_view prefix, std::string_view fallbackId)
    {
        const std::vector<const DialogueLine*> lines = linesWithPrefix(book, prefix);

        if (lines.empty())
        {
            pushSubtitle(subtitles, std::string(fallbackId));
            return;
        }

        for (const DialogueLine* line : lines)
        {
            pushSubtitle(subtitles, subtitleText(*line));
        }
    }

    float linesSeconds(DialogueBook& book, std::string_view prefix, std::string_view fallbackId)
    {
        const std::vector<const DialogueLine*> lines = linesWithPrefix(book, prefix);

        if (lines.empty())
        {
            return subtitleSeconds(fallbackId);
        }

        float seconds = 0.0f;

        for (const DialogueLine* line : lines)
        {
            seconds += subtitleSeconds(subtitleText(*line));
        }

        return seconds;
    }

    bool isSpeaking(const Subtitles& subtitles)
    {
        return !subtitles.queue.empty();
    }

    void stepSubtitles(Subtitles& subtitles, float playerDelta)
    {
        if (subtitles.queue.empty())
        {
            return;
        }

        subtitles.queue.front().seconds -= playerDelta;

        if (subtitles.queue.front().seconds <= 0.0f)
        {
            subtitles.queue.pop_front();
        }
    }

    std::string_view currentSubtitle(const Subtitles& subtitles)
    {
        return subtitles.queue.empty() ? std::string_view{} : std::string_view(subtitles.queue.front().text);
    }
} //namespace game
