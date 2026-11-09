#ifndef LOCKSTEP_PHYSICS_PHYSX_ERROR_REPORTER_HPP
#define LOCKSTEP_PHYSICS_PHYSX_ERROR_REPORTER_HPP

#include <PxPhysicsAPI.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace lockstep::physics
{
    // Receives PhysX's error reports (possibly from worker threads): prints them to stderr,
    // counts errors and warnings, and keeps the last error so a failed create can explain itself.
    class PhysXErrorReporter final : public physx::PxErrorCallback
    {
    public:
        // Called by PhysX for every report.
        // code:    severity and kind.
        // message: the report.
        // file:    PhysX source file that reported it.
        // line:    line in `file`.
        void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override;

        // Returns: errors reported so far.
        [[nodiscard]] uint32_t errorCount() const noexcept;

        // Returns: warnings reported so far.
        [[nodiscard]] uint32_t warningCount() const noexcept;

        // Returns: the last error's message, or an empty string if there was none.
        [[nodiscard]] std::string lastError() const;

        // Builds the message of a failed PhysX call.
        // what:    what failed, e.g. "PxCreatePhysics failed".
        // Returns: `what`, followed by the last PhysX error if there is one.
        [[nodiscard]] std::string describeFailure(const char* what) const;

    private:
        std::atomic<uint32_t> m_errorCount{0};
        std::atomic<uint32_t> m_warningCount{0};
        mutable std::mutex m_lastErrorMutex;
        std::string m_lastError;
    };
} //namespace lockstep::physics

#endif //LOCKSTEP_PHYSICS_PHYSX_ERROR_REPORTER_HPP
