#include "Shell/Credits.hpp"

#include <Lockstep/Assets/FileSystem.hpp>

#include <fstream>
#include <sstream>
#include <string_view>

namespace game
{
    namespace
    {
        constexpr std::size_t k_wrapColumns{92};
        constexpr std::string_view k_fmodLine{"Made with FMOD Studio by Firelight Technologies Pty Ltd."};

        std::string_view trimmed(std::string_view text)
        {
            const std::size_t first = text.find_first_not_of(" \t\r");

            if (first == std::string_view::npos)
            {
                return {};
            }

            return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
        }

        std::string withoutMarkup(std::string_view text)
        {
            std::string result;

            for (const char character : text)
            {
                if (character != '`' && character != '*')
                {
                    result.push_back(character);
                }
            }

            return result;
        }

        void addWrapped(std::vector<CreditLine>& lines, CreditKind kind, std::string_view text)
        {
            std::string current;
            std::istringstream words{std::string(text)};
            std::string word;

            while (words >> word)
            {
                if (!current.empty() && current.size() + 1 + word.size() > k_wrapColumns)
                {
                    lines.push_back(CreditLine{.kind = kind, .text = current});
                    current.clear();
                }

                current += current.empty() ? word : " " + word;
            }

            if (!current.empty())
            {
                lines.push_back(CreditLine{.kind = kind, .text = current});
            }
        }

        std::vector<std::string> cells(std::string_view row)
        {
            std::vector<std::string> result;
            std::string_view rest = trimmed(row);
            rest.remove_prefix(rest.starts_with('|') ? 1 : 0);

            while (!rest.empty())
            {
                const std::size_t bar = rest.find('|');
                result.push_back(withoutMarkup(trimmed(rest.substr(0, bar))));
                rest = bar == std::string_view::npos ? std::string_view() : rest.substr(bar + 1);
            }

            return result;
        }

        void addTableRow(std::vector<CreditLine>& lines, const std::vector<std::string>& row)
        {
            if (row.empty() || row.front().empty())
            {
                return;
            }

            lines.push_back(CreditLine{.kind = CreditKind::Name, .text = row.front()});
            std::string detail;

            for (std::size_t index = 1; index < row.size(); ++index)
            {
                if (!row[index].empty())
                {
                    detail += detail.empty() ? row[index] : "  /  " + row[index];
                }
            }

            addWrapped(lines, CreditKind::Detail, detail);
        }

        void addMarkdown(std::vector<CreditLine>& lines, std::istream& stream)
        {
            std::string text;
            bool isHeaderRow = false;

            while (std::getline(stream, text))
            {
                const std::string_view line = trimmed(text);

                if (line.starts_with("#"))
                {
                    const std::size_t start = line.find_first_not_of('#');
                    lines.push_back(CreditLine{.kind = CreditKind::Gap, .text = {}});
                    lines.push_back(CreditLine{.kind = CreditKind::Title,
                        .text = withoutMarkup(trimmed(line.substr(start == std::string_view::npos ? line.size() : start)))});
                    isHeaderRow = true;
                    continue;
                }

                if (line.starts_with("|"))
                {
                    if (line.find("---") != std::string_view::npos)
                    {
                        continue;
                    }

                    if (isHeaderRow)
                    {
                        isHeaderRow = false;
                        continue;
                    }

                    addTableRow(lines, cells(line));
                    continue;
                }

                if (!line.empty())
                {
                    addWrapped(lines, CreditKind::Detail, withoutMarkup(line));
                }
            }
        }
    } //namespace

    std::vector<CreditLine> loadCredits(const std::filesystem::path& creditsFile,
        const lockstep::assets::FileSystem& files)
    {
        std::vector<CreditLine> lines{
            {CreditKind::Title, "LOCKSTEP"},
            {CreditKind::Detail, "A two-player co-op horror shooter."},
            {CreditKind::Gap, {}},
            {CreditKind::Name, std::string(k_fmodLine)},
            {CreditKind::Detail, "FMOD Engine (c) Firelight Technologies Pty Ltd."}};
        std::ifstream stream(creditsFile);

        if (stream)
        {
            addMarkdown(lines, stream);
        }
        else
        {
            lines.push_back(CreditLine{.kind = CreditKind::Detail, .text = "(CREDITS.md not found: " + creditsFile.string() + ")"});
        }

        if (auto sounds = files.read("sounds/CREDITS.txt"))
        {
            lines.push_back(CreditLine{.kind = CreditKind::Gap, .text = {}});
            lines.push_back(CreditLine{.kind = CreditKind::Title, .text = "Sound notes"});
            const std::vector<std::byte>& bytes = sounds.value();
            std::istringstream soundStream(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
            addMarkdown(lines, soundStream);
        }

        lines.push_back(CreditLine{.kind = CreditKind::Gap, .text = {}});
        lines.push_back(CreditLine{.kind = CreditKind::Detail, .text = "Thank you for playing."});

        return lines;
    }
} //namespace game
