#pragma once

#include "core/buffers/buffers.h"
#include "core/tier1/symspell_dict.h"
#include "core/config/app_config.h"
#include <string>
#include <optional>

class GhostPredictor {
public:
    explicit GhostPredictor(const SymSpellDict& dict);
    std::optional<std::wstring> predictNext(const ContextWindow& context, const CurrentWordBuffer& currentWord, const AppConfig& cfg);

private:
    const SymSpellDict& dict_;
};
