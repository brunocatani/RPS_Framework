#pragma once

#include "RPS/Runtime/PhysicsTypes.h"
#include "RPS/Runtime/RuntimeModule.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace RPS::Runtime::Physics
{
    inline constexpr std::uint32_t GeneratedBodyRetirementGraceSteps = 8;
    inline constexpr std::size_t GeneratedBodyRetirementCapacity = 512;
    inline constexpr std::size_t GeneratedBodyMaximumNameBytes = 127;

    [[nodiscard]] constexpr std::uint32_t advanceGeneratedBodyRetirement(
        const std::uint32_t remainingSteps,
        const std::uint32_t completedPhysicsSteps) noexcept
    {
        return remainingSteps > completedPhysicsSteps ? remainingSteps - completedPhysicsSteps : 0;
    }

    enum class GeneratedMotionType : std::uint8_t
    {
        Static,
        Dynamic,
        Keyframed,
    };

    enum class GeneratedBodyCreateError : std::uint8_t
    {
        None,
        InvalidRuntime,
        WrongThread,
        InvalidInput,
        NameAllocationFailed,
        HavokAllocationFailed,
        NativeConstructionFailed,
        BethesdaAllocationFailed,
        SceneOwnerCreationFailed,
        WorldInsertionFailed,
        InvalidBodyId,
        InvalidMotion,
        MaterialAssignmentFailed,
        WorldIdentityMismatch,
        FinalSnapshotFailed,
    };

    [[nodiscard]] std::string_view toString(GeneratedBodyCreateError error) noexcept;

    struct GeneratedBodyCreateInfo
    {
        void* hknpWorld{};
        void* bhkWorld{};
        const void* shape{};
        std::uint32_t collisionFilterInfo{};
        std::uint16_t materialId{};
        GeneratedMotionType motionType{ GeneratedMotionType::Keyframed };
        std::string_view name{ "RPS_Body" };
    };

    namespace Detail
    {
        class GeneratedBodyState;
    }

    class GeneratedBody
    {
    public:
        GeneratedBody() = default;
        ~GeneratedBody() noexcept;

        GeneratedBody(const GeneratedBody&) = delete;
        GeneratedBody& operator=(const GeneratedBody&) = delete;
        GeneratedBody(GeneratedBody&& other) noexcept;
        GeneratedBody& operator=(GeneratedBody&& other) noexcept;

        [[nodiscard]] bool valid() const noexcept { return _collisionObject != nullptr && _bodyId.valid(); }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] BodyId bodyId() const noexcept { return _bodyId; }
        [[nodiscard]] void* collisionObject() const noexcept { return _collisionObject; }
        [[nodiscard]] void* physicsSystem() const noexcept { return _physicsSystem; }
        [[nodiscard]] void* ownerNode() const noexcept { return _niNode; }
        [[nodiscard]] void* hknpWorld() const noexcept { return _hknpWorld; }
        [[nodiscard]] void* bhkWorld() const noexcept { return _bhkWorld; }

    private:
        friend class GeneratedBodyService;
        friend class Detail::GeneratedBodyState;

        void retireOrQueue() noexcept;
        void clearWithoutRelease() noexcept;
        void moveFrom(GeneratedBody&& other) noexcept;

        std::shared_ptr<Detail::GeneratedBodyState> _state{};
        std::unique_ptr<std::string> _nameStorage{};
        void* _collisionObject{};
        void* _physicsSystem{};
        void* _niNode{};
        void* _hknpWorld{};
        void* _bhkWorld{};
        BodyId _bodyId{};
    };

    class GeneratedBodyService
    {
    public:
        /*
         * The constructing thread is the scene-owner thread for this service.
         * Creation and explicit retirement fail closed on every other thread.
         * A GeneratedBody destroyed elsewhere is placed in the fixed pending
         * queue; call serviceOwnerThreadRetirements() from the owner thread.
         *
         * After native world removal, call serviceCompletedPhysicsSteps() from
         * post-solve once per completed step. Native object references are held
         * for eight real broadphase rebuilds before release. Only use
         * shutdownAfterWorldLoss() when no later physics reader can run.
         */
        GeneratedBodyService() = default;
        explicit GeneratedBodyService(RuntimeModule module) noexcept;

        GeneratedBodyService(const GeneratedBodyService&) = delete;
        GeneratedBodyService& operator=(const GeneratedBodyService&) = delete;
        GeneratedBodyService(GeneratedBodyService&&) noexcept = default;
        GeneratedBodyService& operator=(GeneratedBodyService&&) noexcept = default;

        [[nodiscard]] bool ready() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept { return ready(); }
        [[nodiscard]] GeneratedBody create(
            const GeneratedBodyCreateInfo& info,
            GeneratedBodyCreateError& error) noexcept;
        [[nodiscard]] bool retire(GeneratedBody& body) noexcept;
        [[nodiscard]] std::size_t serviceOwnerThreadRetirements() noexcept;
        [[nodiscard]] std::size_t serviceCompletedPhysicsSteps(std::uint32_t completedPhysicsSteps = 1) noexcept;
        [[nodiscard]] std::size_t shutdownAfterWorldLoss() noexcept;
        [[nodiscard]] std::size_t retirementCount() const noexcept;

    private:
        std::shared_ptr<Detail::GeneratedBodyState> _state{};
    };
}
