//
// Created by Orgest on 4/18/2026.
//

#pragma once
#include "glm/glm.hpp"

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;

    Ray() = default;

    Ray(const glm::vec3& origin, const glm::vec3& direction)
    {
        this->origin = origin;
        this->direction = glm::normalize(direction);
    }
};
