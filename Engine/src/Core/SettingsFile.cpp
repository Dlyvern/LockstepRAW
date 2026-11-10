#include "Lockstep/Core/SettingsFile.hpp"

#include <fstream>
#include <system_error>
#include <utility>

namespace lockstep
{
    namespace
    {
        std::string_view trimmed(std::string_view text)
        {
            const std::size_t first = text.find_first_not_of(" \t\r");

            if (first == std::string_view::npos)
            {
                return {};
            }

            const std::size_t last = text.find_last_not_of(" \t\r");

            return text.substr(first, last - first + 1);
        }
    } //namespace

    SettingsFile SettingsFile::load(const std::filesystem::path& file)
    {
        SettingsFile settings;
        settings.m_file = file;
        std::ifstream stream(file);
        std::string text;
        std::string section;

        while (std::getline(stream, text))
        {
            Line line{.text = text, .section = section, .key = {}, .value = {}, .isHeader = false};
            const std::string_view content = trimmed(std::string_view(text).substr(0, text.find('#')));

            if (content.size() >= 2 && content.front() == '[' && content.back() == ']')
            {
                section = std::string(trimmed(content.substr(1, content.size() - 2)));
                line.section = section;
                line.isHeader = true;
            }
            else if (const std::size_t equals = content.find('='); equals != std::string_view::npos)
            {
                line.key = std::string(trimmed(content.substr(0, equals)));
                line.value = std::string(trimmed(content.substr(equals + 1)));
            }

            settings.m_lines.push_back(std::move(line));
        }

        return settings;
    }

    std::optional<std::string_view> SettingsFile::value(std::string_view section, std::string_view key) const
    {
        std::optional<std::string_view> found;

        for (const Line& line : m_lines)
        {
            if (!line.isHeader && !line.key.empty() && line.section == section && line.key == key)
            {
                found = line.value;
            }
        }

        return found;
    }

    bool SettingsFile::contains(std::string_view section, std::string_view key) const
    {
        return value(section, key).has_value();
    }

    void SettingsFile::set(std::string_view section, std::string_view key, std::string_view value)
    {
        const std::string text = std::string(key) + " = " + std::string(value);
        std::optional<std::size_t> lastInSection;

        for (std::size_t index = 0; index < m_lines.size(); ++index)
        {
            Line& line = m_lines[index];

            if (line.section != section)
            {
                continue;
            }

            if (!line.isHeader && line.key == key)
            {
                line.text = text;
                line.value = std::string(value);
                return;
            }

            if (line.isHeader || !trimmed(line.text).empty())
            {
                lastInSection = index;
            }
        }

        Line line{.text = text, .section = std::string(section), .key = std::string(key), .value = std::string(value)};

        if (lastInSection)
        {
            m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(*lastInSection + 1), std::move(line));
            return;
        }

        if (section.empty())
        {
            m_lines.insert(m_lines.begin(), std::move(line));
            return;
        }

        m_lines.push_back(Line{.text = "[" + std::string(section) + "]", .section = std::string(section), .key = {},
            .value = {}, .isHeader = true});
        m_lines.push_back(std::move(line));
    }

    Status SettingsFile::save() const
    {
        std::error_code error;

        if (m_file.has_parent_path())
        {
            std::filesystem::create_directories(m_file.parent_path(), error);
        }

        std::ofstream stream(m_file, std::ios::trunc);

        if (!stream)
        {
            return Error{ErrorCode::PlatformFailure, "SettingsFile: cannot write '" + m_file.string() + "'"};
        }

        for (const Line& line : m_lines)
        {
            stream << line.text << '\n';
        }

        if (!stream)
        {
            return Error{ErrorCode::PlatformFailure, "SettingsFile: writing '" + m_file.string() + "' failed"};
        }

        return {};
    }

    const std::filesystem::path& SettingsFile::path() const noexcept
    {
        return m_file;
    }
} //namespace lockstep
