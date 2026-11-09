#include "Echo/EchoRecording.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

namespace game
{
    namespace
    {
        constexpr std::array<char, 4> k_magic{'L', 'E', 'C', 'H'};
        constexpr uint32_t k_version{3};
        constexpr const char* k_appFolder{"LockstepRAW"};
        constexpr std::size_t k_headerBytes{20};
        constexpr std::size_t k_sampleBytes{1 + 5 * sizeof(float)};
        constexpr std::size_t k_eventBytes{sizeof(uint32_t) + 3};
        constexpr std::string_view k_saveFlag{"--echo-file"};
        constexpr std::string_view k_playFlag{"--echo-play"};

        lockstep::Error echoError(const std::filesystem::path& path, std::string_view what)
        {
            return lockstep::Error{lockstep::ErrorCode::AssetLoadFailed,
                "echo recording '" + path.string() + "': " + std::string(what)};
        }

        class Writer
        {
        public:
            void bytes(std::span<const char> data)
            {
                m_bytes.insert(m_bytes.end(), data.begin(), data.end());
            }

            void u8(uint8_t value)
            {
                m_bytes.push_back(static_cast<char>(value));
            }

            void u32(uint32_t value)
            {
                for (int shift = 0; shift < 32; shift += 8)
                {
                    u8(static_cast<uint8_t>((value >> shift) & 0xFFu));
                }
            }

            void f32(float value)
            {
                u32(std::bit_cast<uint32_t>(value));
            }

            [[nodiscard]] const std::vector<char>& data() const
            {
                return m_bytes;
            }

        private:
            std::vector<char> m_bytes;
        };

        class Reader
        {
        public:
            explicit Reader(std::span<const char> bytes)
                : m_bytes(bytes)
            {
            }

            [[nodiscard]] bool has(std::size_t count) const
            {
                return m_offset + count <= m_bytes.size();
            }

            uint8_t u8()
            {
                return has(1) ? static_cast<uint8_t>(m_bytes[m_offset++]) : 0;
            }

            uint32_t u32()
            {
                uint32_t value = 0;

                for (int shift = 0; shift < 32; shift += 8)
                {
                    value |= static_cast<uint32_t>(u8()) << shift;
                }

                return value;
            }

            float f32()
            {
                return std::bit_cast<float>(u32());
            }

            [[nodiscard]] std::size_t remaining() const
            {
                return m_bytes.size() - m_offset;
            }

        private:
            std::span<const char> m_bytes;
            std::size_t m_offset{0};
        };

        bool readSample(Reader& reader, EchoPawnSample& sample)
        {
            sample.isPresent = reader.u8() != 0;
            sample.foot = math::Vec3{reader.f32(), reader.f32(), reader.f32()};
            sample.yaw = reader.f32();
            sample.pitch = reader.f32();

            return std::isfinite(sample.foot.x) && std::isfinite(sample.foot.y) && std::isfinite(sample.foot.z)
                && std::isfinite(sample.yaw) && std::isfinite(sample.pitch);
        }

        lockstep::Result<std::vector<char>> readFile(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);

            if (!file)
            {
                return echoError(path, "can't open the file");
            }

            return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }

        std::optional<std::filesystem::path> environmentPath(const char* name)
        {
#ifdef _WIN32
            char* value = nullptr;
            std::size_t length = 0;

            if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
            {
                return std::nullopt;
            }

            std::filesystem::path path(value);
            std::free(value);
#else
            const char* value = std::getenv(name);

            if (value == nullptr)
            {
                return std::nullopt;
            }

            std::filesystem::path path(value);
#endif

            if (path.empty() || !path.is_absolute())
            {
                return std::nullopt;
            }

            return path;
        }

        std::filesystem::path userDataFolder()
        {
#ifdef _WIN32
            if (const std::optional<std::filesystem::path> appData = environmentPath("APPDATA"))
            {
                return *appData / k_appFolder;
            }
#else
            if (const std::optional<std::filesystem::path> dataHome = environmentPath("XDG_DATA_HOME"))
            {
                return *dataHome / k_appFolder;
            }

            if (const std::optional<std::filesystem::path> home = environmentPath("HOME"))
            {
                return *home / ".local" / "share" / k_appFolder;
            }
#endif

            return std::filesystem::path("Saved");
        }

        float lerpAngle(float from, float to, float t)
        {
            const float difference = std::remainder(to - from, 2.0f * math::k_pi);
            return from + difference * t;
        }
    } //namespace

    lockstep::Status saveEchoRecording(const EchoRecording& recording, const std::filesystem::path& path)
    {
        Writer writer;
        writer.bytes(k_magic);
        writer.u32(k_version);
        writer.u32(recording.tickRate);
        writer.u32(static_cast<uint32_t>(recording.frames.size()));
        writer.u32(static_cast<uint32_t>(recording.events.size()));

        for (const EchoFrame& frame : recording.frames)
        {
            for (const EchoPawnSample& sample : frame.pawns)
            {
                writer.u8(sample.isPresent ? 1 : 0);
                writer.f32(sample.foot.x);
                writer.f32(sample.foot.y);
                writer.f32(sample.foot.z);
                writer.f32(sample.yaw);
                writer.f32(sample.pitch);
            }
        }

        for (const EchoEvent& event : recording.events)
        {
            writer.u32(event.frame);
            writer.u8(event.player);
            writer.u8(static_cast<uint8_t>(event.object));
            writer.u8(static_cast<uint8_t>(event.action));
        }

        std::error_code error;

        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), error);
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(writer.data().data(), static_cast<std::streamsize>(writer.data().size()));

        if (!file)
        {
            return lockstep::Error{lockstep::ErrorCode::PlatformFailure,
                "echo recording '" + path.string() + "': can't write the file"};
        }

        return {};
    }

    lockstep::Result<EchoRecording> loadEchoRecording(const std::filesystem::path& path)
    {
        auto bytes = readFile(path);

        if (!bytes)
        {
            return bytes.error();
        }

        Reader reader(bytes.value());

        if (!reader.has(k_headerBytes) || std::memcmp(bytes.value().data(), k_magic.data(), k_magic.size()) != 0)
        {
            return echoError(path, "not an echo recording");
        }

        for (std::size_t index = 0; index < k_magic.size(); ++index)
        {
            reader.u8();
        }

        EchoRecording recording{};

        if (reader.u32() != k_version)
        {
            return echoError(path, "unsupported version");
        }

        recording.tickRate = reader.u32();
        const uint32_t frameCount = reader.u32();
        const uint32_t eventCount = reader.u32();
        const std::size_t expected = static_cast<std::size_t>(frameCount) * k_playerCount * k_sampleBytes
            + static_cast<std::size_t>(eventCount) * k_eventBytes;

        if (reader.remaining() != expected || recording.tickRate == 0)
        {
            return echoError(path, "the file is truncated or damaged");
        }

        recording.frames.resize(frameCount);

        for (EchoFrame& frame : recording.frames)
        {
            for (EchoPawnSample& sample : frame.pawns)
            {
                if (!readSample(reader, sample))
                {
                    return echoError(path, "a sample is not a finite number");
                }
            }
        }

        recording.events.resize(eventCount);

        for (EchoEvent& event : recording.events)
        {
            event.frame = reader.u32();
            event.player = reader.u8();
            event.object = static_cast<RoomObject>(reader.u8());
            event.action = static_cast<RoomAction>(reader.u8());

            if (event.frame >= frameCount || event.player >= k_playerCount
                || event.object > RoomObject::Other || event.action > RoomAction::Refused)
            {
                return echoError(path, "an event is out of range");
            }
        }

        return recording;
    }

    void stepEchoPlayback(EchoPlayback& playback)
    {
        const std::size_t frameCount = playback.recording.frames.size();

        if (frameCount == 0)
        {
            return;
        }

        playback.frame = (playback.frame + 1) % frameCount;

        if (playback.frame == 0)
        {
            playback.nextEvent = 0;
            std::clog << "Game: echo loops\n";
        }

        const std::vector<EchoEvent>& events = playback.recording.events;

        while (playback.nextEvent < events.size() && events[playback.nextEvent].frame <= playback.frame)
        {
            const EchoEvent& event = events[playback.nextEvent++];
            std::clog << "Game: echo of " << playerName(event.player) << ": " << toString(event.action) << " ("
                << toString(event.object) << ")\n";
        }
    }

    std::array<std::optional<math::Transform>, k_playerCount> echoGhostTransforms(const EchoPlayback& playback,
        float alpha)
    {
        std::array<std::optional<math::Transform>, k_playerCount> ghosts{};
        const std::vector<EchoFrame>& frames = playback.recording.frames;

        if (frames.empty())
        {
            return ghosts;
        }

        const EchoFrame& current = frames[playback.frame];
        const EchoFrame& previous = frames[playback.frame == 0 ? 0 : playback.frame - 1];

        for (std::size_t player = 0; player < k_playerCount; ++player)
        {
            const EchoPawnSample& to = current.pawns[player];
            const EchoPawnSample& from = previous.pawns[player].isPresent ? previous.pawns[player] : to;

            if (!to.isPresent)
            {
                continue;
            }

            const math::Vec3 foot = math::lerp(from.foot, to.foot, alpha);
            const float yaw = lerpAngle(from.yaw, to.yaw, alpha);
            ghosts[player] = math::Transform{.position = foot + math::Vec3{0.0f, 0.5f * k_pawnStandHeight, 0.0f},
                .rotation = math::angleAxis(yaw, math::Vec3{0.0f, 1.0f, 0.0f}),
                .scale = math::Vec3{2.0f * k_pawnRadius, k_pawnStandHeight, 2.0f * k_pawnRadius}};
        }

        return ghosts;
    }

    std::filesystem::path defaultEchoPath()
    {
        return userDataFolder() / k_echoFileName;
    }

    lockstep::Result<EchoLaunch> parseEchoLaunch(std::span<const char* const> arguments)
    {
        EchoLaunch launch{};
        launch.savePath = defaultEchoPath();

        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string_view argument = arguments[index] != nullptr ? arguments[index] : "";

            if (argument != k_saveFlag && argument != k_playFlag)
            {
                continue;
            }

            if (index + 1 >= arguments.size() || arguments[index + 1] == nullptr)
            {
                return lockstep::Error{lockstep::ErrorCode::InvalidArgument, std::string(argument) + " needs a value"};
            }

            const std::filesystem::path value(arguments[++index]);

            if (argument == k_saveFlag)
            {
                launch.savePath = value;
            }
            else
            {
                launch.playPath = value;
            }
        }

        return launch;
    }
} //namespace game
