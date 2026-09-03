#pragma once

#include <app/clusters/mode-select-server/supported-modes-manager.h>

#include <vector>

#include "led.h"

namespace chip {
namespace app {
namespace Clusters {
namespace ModeSelect {

/**
 * This implementation statically defines the options for the Light ModeSelect cluster.
 */
class DynamicSupportedModesManager : public SupportedModesManager {
   private:
    mutable std::vector<Structs::ModeOptionStruct::Type> options;
    mutable bool options_built = false;

    void ensure_options() const {
        if (options_built) return;
        options.reserve(modes.size());
        for (auto& m : modes)
            options.push_back({.label = chip::CharSpan::fromCharString(m.name), .mode = m.id, .semanticTags = {}});
        options_built = true;
    }

   public:
    ModeOptionsProvider getModeOptionsProvider(EndpointId endpointId) const override {
        ensure_options();
        return ModeOptionsProvider(options.data(), options.data() + options.size());
    }

    Protocols::InteractionModel::Status getModeOptionByMode(
        EndpointId endpointId, uint8_t mode, const Structs::ModeOptionStruct::Type** dataPtr) const override {
        ensure_options();
        for (const auto& option : options) {
            if (option.mode == mode) {
                *dataPtr = &option;
                return Protocols::InteractionModel::Status::Success;
            }
        }
        return Protocols::InteractionModel::Status::InvalidCommand;
    }

    ~DynamicSupportedModesManager() = default;
    DynamicSupportedModesManager() = default;
};

// Single global instance used by app_main; defined in mode_select_driver.cpp
extern DynamicSupportedModesManager gStaticSupportedModesManager;

}  // namespace ModeSelect
}  // namespace Clusters
}  // namespace app
}  // namespace chip