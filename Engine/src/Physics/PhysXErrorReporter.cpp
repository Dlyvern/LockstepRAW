#include "PhysXErrorReporter.hpp"

#include <cstdio>

namespace lockstep::physics
{
    namespace
    {
        bool isWarning(physx::PxErrorCode::Enum code)
        {
            return code == physx::PxErrorCode::eDEBUG_WARNING || code == physx::PxErrorCode::ePERF_WARNING;
        }
    } //namespace

    void PhysXErrorReporter::reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
    {
        if (code == physx::PxErrorCode::eDEBUG_INFO)
        {
            std::fprintf(stderr, "[PhysX] info: %s\n", message);
            return;
        }

        if (isWarning(code))
        {
            m_warningCount.fetch_add(1, std::memory_order_relaxed);
            std::fprintf(stderr, "[PhysX] warning: %s (%s:%d)\n", message, file, line);
            return;
        }

        m_errorCount.fetch_add(1, std::memory_order_relaxed);
        std::fprintf(stderr, "[PhysX] error %d: %s (%s:%d)\n", static_cast<int>(code), message, file, line);

        const std::lock_guard lock(m_lastErrorMutex);
        m_lastError = message != nullptr ? message : "";
    }

    uint32_t PhysXErrorReporter::errorCount() const noexcept
    {
        return m_errorCount.load(std::memory_order_relaxed);
    }

    uint32_t PhysXErrorReporter::warningCount() const noexcept
    {
        return m_warningCount.load(std::memory_order_relaxed);
    }

    std::string PhysXErrorReporter::lastError() const
    {
        const std::lock_guard lock(m_lastErrorMutex);
        return m_lastError;
    }

    std::string PhysXErrorReporter::describeFailure(const char* what) const
    {
        std::string description = what;
        const std::string physxError = lastError();

        if (!physxError.empty())
        {
            description += ": ";
            description += physxError;
        }

        return description;
    }
} //namespace lockstep::physics
