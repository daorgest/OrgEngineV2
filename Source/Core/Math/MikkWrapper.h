//
// Created by Orgest on 8/28/2025.
//

#pragma once
#include "../../Engine/MeshData.h"

void GenerateMikkTangents(const std::span<Vertex> vertices, const std::span<const uint32_t> indices);