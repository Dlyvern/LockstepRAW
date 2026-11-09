#include "Lockstep/Core/Result.hpp"

namespace lockstep
{
    const char* toString(ErrorCode code) noexcept
    {
        switch (code)
        {
            case ErrorCode::Unknown:
            {
                return "Unknown";
            }
            case ErrorCode::InvalidArgument:
            {
                return "InvalidArgument";
            }
            case ErrorCode::InvalidHandle:
            {
                return "InvalidHandle";
            }
            case ErrorCode::PlatformFailure:
            {
                return "PlatformFailure";
            }
            case ErrorCode::OutOfMemory:
            {
                return "OutOfMemory";
            }
            case ErrorCode::AssetLoadFailed:
            {
                return "AssetLoadFailed";
            }
            case ErrorCode::GpuFailure:
            {
                return "GpuFailure";
            }
            case ErrorCode::NetworkFailure:
            {
                return "NetworkFailure";
            }
            case ErrorCode::PhysicsFailure:
            {
                return "PhysicsFailure";
            }
        }

        return "Unknown";
    }
} //namespace lockstep
