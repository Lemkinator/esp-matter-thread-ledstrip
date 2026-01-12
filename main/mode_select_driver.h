#pragma once

#include <app/clusters/mode-select-server/supported-modes-manager.h>

namespace chip {
namespace app {
namespace Clusters {
namespace ModeSelect {

/**
 * This implementation statically defines the options for the Light ModeSelect cluster.
 */
class StaticSupportedModesManager : public SupportedModesManager
{
private:
    // Make the array static inline to allow definition and initialization in the header
    static inline const Structs::ModeOptionStruct::Type lightModes[] = {
        {.label = chip::CharSpan::fromCharString("Solid"), .mode = 0, .semanticTags = {/* empty tags */}},
        {.label = chip::CharSpan::fromCharString("Blaze"), .mode = 10, .semanticTags = {/* empty tags */}},
        {.label = chip::CharSpan::fromCharString("Fade"), .mode = 20, .semanticTags = {/* empty tags */}},
        {.label = chip::CharSpan::fromCharString("Pulse"), .mode = 30, .semanticTags = {/* empty tags */}},
        {.label = chip::CharSpan::fromCharString("Rainbow"), .mode = 40, .semanticTags = {/* empty tags */}}
    };

public:
    // The methods must also be defined inline (either explicitly or implicitly
    // by defining them inside the class definition).
    ModeOptionsProvider getModeOptionsProvider(EndpointId endpointId) const override
    {
        return ModeOptionsProvider(lightModes, lightModes + sizeof(lightModes) / sizeof(lightModes[0]));
    }

    Protocols::InteractionModel::Status getModeOptionByMode(
        EndpointId endpointId, uint8_t mode, const Structs::ModeOptionStruct::Type **dataPtr) const override
    {
        for (const auto &option : lightModes)
        {
            if (option.mode == mode)
            {
                *dataPtr = &option;
                return Protocols::InteractionModel::Status::Success;
            }
        }
        return Protocols::InteractionModel::Status::InvalidCommand;
    }

    ~StaticSupportedModesManager() = default;
    StaticSupportedModesManager() = default;
};

// Define the single instance in the header using static inline
static inline StaticSupportedModesManager gStaticSupportedModesManager;

} // namespace ModeSelect
} // namespace Clusters
} // namespace app
} // namespace chip