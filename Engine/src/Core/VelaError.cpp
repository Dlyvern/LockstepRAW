#include "VelaError.hpp"

namespace lockstep
{
    namespace
    {
        ErrorCode toErrorCode(vela::ErrorCode code)
        {
            switch (code)
            {
                case vela::ErrorCode::InvalidArgument:
                {
                    return ErrorCode::InvalidArgument;
                }
                case vela::ErrorCode::WindowCreationFailed:
                case vela::ErrorCode::NoSuitableDevice:
                case vela::ErrorCode::DeviceCreationFailed:
                case vela::ErrorCode::SurfaceCreationFailed:
                case vela::ErrorCode::SwapchainCreationFailed:
                {
                    return ErrorCode::PlatformFailure;
                }
                case vela::ErrorCode::AllocationFailed:
                {
                    return ErrorCode::OutOfMemory;
                }
                case vela::ErrorCode::ImageLoadFailed:
                case vela::ErrorCode::ShaderLoadFailed:
                {
                    return ErrorCode::AssetLoadFailed;
                }
                case vela::ErrorCode::PipelineCreationFailed:
                case vela::ErrorCode::DescriptorCreationFailed:
                {
                    return ErrorCode::GpuFailure;
                }
                case vela::ErrorCode::Unknown:
                {
                    return ErrorCode::Unknown;
                }
            }

            return ErrorCode::Unknown;
        }
    } //namespace

    Error toError(const vela::Error& error)
    {
        return Error{toErrorCode(error.code), "Vela: " + error.message};
    }
} //namespace lockstep
