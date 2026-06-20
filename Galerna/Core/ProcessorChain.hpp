#pragma once

#include <tuple>
#include <utility>

namespace galerna::core
{

template <typename... Processors>
class ProcessorChain
{
public:
    explicit ProcessorChain(Processors&... processors)
        : processors_{processors...}
    {
    }

    template <typename AudioBuffer>
    void processBlock(AudioBuffer& buffer)
    {
        std::apply(
            [&buffer](auto&... processor)
            {
                (processor.processBlock(buffer), ...);
            },
            processors_);
    }

private:
    std::tuple<Processors&...> processors_;
};

} // namespace galerna::core
