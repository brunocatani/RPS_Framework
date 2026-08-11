#pragma once

namespace RPS::Runtime
{
    [[nodiscard]] bool addHavokReference(const void* object) noexcept;
    [[nodiscard]] bool addBethesdaReference(const void* object) noexcept;
    [[nodiscard]] bool releaseHavokReference(void* object) noexcept;
    [[nodiscard]] bool releaseBethesdaReference(void* object) noexcept;
}
