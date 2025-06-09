//
// Created by Orgest on 6/9/2025.
//

#pragma once
#include <fmt/core.h>

#define Log(fmt_str, ...) \
fmt::print("[LOG] " fmt_str "\n", ##__VA_ARGS__)
