#ifndef LOCKSTEP_ASSETS_IMPORT_WARNINGS_HPP
#define LOCKSTEP_ASSETS_IMPORT_WARNINGS_HPP

#include <string>
#include <unordered_set>
#include <vector>

namespace lockstep::assets
{
    // Collects what an import skipped or approximated, each distinct message once, in the order
    // first seen.
    class ImportWarnings
    {
    public:
        // Records a warning unless the same text was recorded before.
        // message: one line, no trailing newline.
        void add(std::string message)
        {
            if (m_seen.insert(message).second)
            {
                m_messages.push_back(std::move(message));
            }
        }

        // Returns: the warnings so far; the collector is empty afterwards.
        [[nodiscard]] std::vector<std::string> take()
        {
            m_seen.clear();
            return std::move(m_messages);
        }

    private:
        std::unordered_set<std::string> m_seen;
        std::vector<std::string> m_messages;
    };
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_IMPORT_WARNINGS_HPP
