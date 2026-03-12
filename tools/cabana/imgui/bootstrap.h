#pragma once

#include <string>

#include "tools/cabana/core/launch_config.h"
#include "tools/cabana/imgui/stream.h"

AbstractStream *createStreamForLaunchConfig(const CabanaLaunchConfig &config, std::string *error = nullptr);
