#ifndef LOCKSTEP_CORE_RESULT_HPP
#define LOCKSTEP_CORE_RESULT_HPP

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

namespace lockstep
{
    // Broad category of a failure, for code that wants to react to the kind of error.
    enum class ErrorCode : uint32_t
    {
        Unknown = 0,
        InvalidArgument,
        InvalidHandle,
        PlatformFailure,
        OutOfMemory,
        AssetLoadFailed,
        GpuFailure,
        NetworkFailure
    };

    // A failure: what kind it was and a human-readable explanation.
    struct Error
    {
        ErrorCode code{ErrorCode::Unknown};
        std::string message;
    };

    // Holds either a value of type T or an Error. Move-only when T is. Marked [[nodiscard]] so a
    // returned Result can never be silently ignored.
    template<typename T>
    class [[nodiscard]] Result
    {
    public:
        // Creates a successful result holding `value`.
        Result(T&& value) noexcept
            : m_storage(std::in_place_index<0>, std::move(value))
        {
        }

        // Creates a failed result holding `error`.
        Result(Error error) noexcept
            : m_storage(std::in_place_index<1>, std::move(error))
        {
        }

        // Returns: true if the result holds a value.
        [[nodiscard]] bool hasValue() const noexcept
        {
            return m_storage.index() == 0;
        }

        // Returns: true if the result holds a value.
        explicit operator bool() const noexcept
        {
            return hasValue();
        }

        // Returns: the held value. Only valid when hasValue() is true.
        [[nodiscard]] T& value() & noexcept
        {
            assert(hasValue());
            return std::get<0>(m_storage);
        }

        // Returns: the held value. Only valid when hasValue() is true.
        [[nodiscard]] const T& value() const& noexcept
        {
            assert(hasValue());
            return std::get<0>(m_storage);
        }

        // Returns: the held value, moved out. Only valid when hasValue() is true.
        [[nodiscard]] T&& value() && noexcept
        {
            assert(hasValue());
            return std::get<0>(std::move(m_storage));
        }

        // Returns: a pointer to the held value. Only valid when hasValue() is true.
        T* operator->() noexcept
        {
            return &value();
        }

        // Returns: a pointer to the held value. Only valid when hasValue() is true.
        const T* operator->() const noexcept
        {
            return &value();
        }

        // Returns: the held error. Only valid when hasValue() is false.
        [[nodiscard]] const Error& error() const noexcept
        {
            assert(!hasValue());
            return std::get<1>(m_storage);
        }

    private:
        std::variant<T, Error> m_storage;
    };

    // The no-value form of Result: an operation either succeeded or explains why not.
    template<>
    class [[nodiscard]] Result<void>
    {
    public:
        // Creates a successful status.
        Result() noexcept = default;

        // Creates a failed status holding `error`.
        Result(Error error) noexcept
            : m_failed(true),
              m_error(std::move(error))
        {
        }

        // Returns: true if the operation succeeded.
        [[nodiscard]] bool hasValue() const noexcept
        {
            return !m_failed;
        }

        // Returns: true if the operation succeeded.
        explicit operator bool() const noexcept
        {
            return hasValue();
        }

        // Returns: the held error. Only valid when hasValue() is false.
        [[nodiscard]] const Error& error() const noexcept
        {
            assert(m_failed);
            return m_error;
        }

    private:
        bool m_failed{false};
        Error m_error;
    };

    // Result of an operation that produces no value.
    using Status = Result<void>;

    // Returns: a readable name for `code`, for logging.
    [[nodiscard]] const char* toString(ErrorCode code) noexcept;
} //namespace lockstep

#endif //LOCKSTEP_CORE_RESULT_HPP
