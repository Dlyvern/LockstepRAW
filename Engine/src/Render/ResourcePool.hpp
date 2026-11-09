#ifndef LOCKSTEP_RENDER_RESOURCE_POOL_HPP
#define LOCKSTEP_RENDER_RESOURCE_POOL_HPP

#include <cstdint>
#include <deque>
#include <utility>

namespace lockstep::render
{
    // Stores GPU resources of one kind and hands out typed handles to them. Backed by a deque so
    // a resource never moves once added: the render scene keeps raw pointers to submitted meshes
    // and materials until the frame is recorded.
    template<typename HandleT, typename ResourceT>
    class ResourcePool
    {
    public:
        // Takes ownership of a resource.
        // resource: the resource to store.
        // Returns:  the handle that now refers to it.
        [[nodiscard]] HandleT add(ResourceT&& resource)
        {
            m_resources.push_back(std::move(resource));

            return HandleT{static_cast<uint32_t>(m_resources.size() - 1)};
        }

        // Looks up a resource.
        // handle:  the handle to resolve.
        // Returns: the resource, or nullptr if the handle is invalid or out of range.
        [[nodiscard]] const ResourceT* find(HandleT handle) const
        {
            if (!handle.isValid() || handle.index >= m_resources.size())
            {
                return nullptr;
            }

            return &m_resources[handle.index];
        }

        // Calls `function` with every resource, in creation order.
        // function: callable taking ResourceT&.
        template<typename Function>
        void forEach(Function&& function)
        {
            for (ResourceT& resource : m_resources)
            {
                function(resource);
            }
        }

    private:
        std::deque<ResourceT> m_resources;
    };
} //namespace lockstep::render

#endif //LOCKSTEP_RENDER_RESOURCE_POOL_HPP
