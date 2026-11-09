#include "GltfSkins.hpp"

#include <algorithm>
#include <optional>

namespace lockstep::assets
{
    namespace
    {
        std::optional<anim::ChannelTarget> toTarget(cgltf_animation_path_type path)
        {
            switch (path)
            {
                case cgltf_animation_path_type_translation:
                {
                    return anim::ChannelTarget::Translation;
                }
                case cgltf_animation_path_type_rotation:
                {
                    return anim::ChannelTarget::Rotation;
                }
                case cgltf_animation_path_type_scale:
                {
                    return anim::ChannelTarget::Scale;
                }
                default:
                {
                    return std::nullopt;
                }
            }
        }

        anim::Interpolation toInterpolation(cgltf_interpolation_type type)
        {
            switch (type)
            {
                case cgltf_interpolation_type_step:
                {
                    return anim::Interpolation::Step;
                }
                case cgltf_interpolation_type_cubic_spline:
                {
                    return anim::Interpolation::CubicSpline;
                }
                default:
                {
                    return anim::Interpolation::Linear;
                }
            }
        }

        std::optional<anim::AnimationChannel> readChannel(const cgltf_animation_channel& source, uint32_t joint,
            anim::ChannelTarget target)
        {
            const cgltf_animation_sampler* sampler = source.sampler;

            if (sampler == nullptr || sampler->input == nullptr || sampler->output == nullptr
                || sampler->input->count == 0)
            {
                return std::nullopt;
            }

            anim::AnimationChannel channel{};
            channel.joint = joint;
            channel.target = target;
            channel.interpolation = toInterpolation(sampler->interpolation);

            const cgltf_accessor& input = *sampler->input;
            const cgltf_accessor& output = *sampler->output;
            const cgltf_size components = cgltf_num_components(output.type);
            const cgltf_size expected = target == anim::ChannelTarget::Rotation ? 4 : 3;
            const cgltf_size stride = channel.interpolation == anim::Interpolation::CubicSpline ? 3 : 1;

            if (input.type != cgltf_type_scalar || components != expected || output.count != input.count * stride)
            {
                return std::nullopt;
            }

            channel.times.resize(input.count);

            if (cgltf_accessor_unpack_floats(&input, channel.times.data(), channel.times.size()) != channel.times.size()
                || !std::is_sorted(channel.times.begin(), channel.times.end()))
            {
                return std::nullopt;
            }

            std::vector<float> values(output.count * components);

            if (cgltf_accessor_unpack_floats(&output, values.data(), values.size()) != values.size())
            {
                return std::nullopt;
            }

            channel.values.resize(output.count);

            for (cgltf_size key = 0; key < output.count; ++key)
            {
                const float* value = values.data() + key * components;
                channel.values[key] = math::Vec4{value[0], value[1], value[2], components == 4 ? value[3] : 0.0f};
            }

            return channel;
        }

        std::string clipName(const cgltf_animation& animation, cgltf_size index)
        {
            return animation.name != nullptr && animation.name[0] != '\0' ? std::string(animation.name)
                : "clip " + std::to_string(index);
        }
    } //namespace

    std::vector<anim::AnimationClip> readClips(const cgltf_data& document, const JointLookup& joints,
        ImportWarnings& warnings)
    {
        std::vector<anim::AnimationClip> clips;

        for (cgltf_size index = 0; index < document.animations_count; ++index)
        {
            const cgltf_animation& animation = document.animations[index];
            anim::AnimationClip clip{};
            clip.name = clipName(animation, index);

            for (cgltf_size channelIndex = 0; channelIndex < animation.channels_count; ++channelIndex)
            {
                const cgltf_animation_channel& source = animation.channels[channelIndex];
                const auto joint = joints.find(source.target_node);

                if (joint == joints.end())
                {
                    continue;
                }

                const std::optional<anim::ChannelTarget> target = toTarget(source.target_path);

                if (!target)
                {
                    warnings.add("morph target animation is ignored");
                    continue;
                }

                std::optional<anim::AnimationChannel> channel = readChannel(source, joint->second, *target);

                if (!channel)
                {
                    warnings.add("animation '" + clip.name + "': a malformed channel is skipped");
                    continue;
                }

                clip.duration = std::max(clip.duration, channel->times.back());
                clip.channels.push_back(std::move(*channel));
            }

            if (!clip.channels.empty())
            {
                clips.push_back(std::move(clip));
            }
        }

        return clips;
    }
} //namespace lockstep::assets
