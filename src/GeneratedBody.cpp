#include "RPS/Runtime/GeneratedBody.h"

#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/BodyAccess.h"
#include "RPS/Runtime/HavokAllocator.h"
#include "RPS/Runtime/Memory.h"
#include "RPS/Runtime/NativeReference.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <utility>

namespace RPS::Runtime::Physics
{
    std::string_view toString(const GeneratedBodyCreateError error) noexcept
    {
        switch (error) {
        case GeneratedBodyCreateError::None:
            return "none";
        case GeneratedBodyCreateError::InvalidRuntime:
            return "invalid-runtime";
        case GeneratedBodyCreateError::WrongThread:
            return "wrong-thread";
        case GeneratedBodyCreateError::InvalidInput:
            return "invalid-input";
        case GeneratedBodyCreateError::NameAllocationFailed:
            return "name-allocation-failed";
        case GeneratedBodyCreateError::HavokAllocationFailed:
            return "havok-allocation-failed";
        case GeneratedBodyCreateError::NativeConstructionFailed:
            return "native-construction-failed";
        case GeneratedBodyCreateError::BethesdaAllocationFailed:
            return "bethesda-allocation-failed";
        case GeneratedBodyCreateError::SceneOwnerCreationFailed:
            return "scene-owner-creation-failed";
        case GeneratedBodyCreateError::WorldInsertionFailed:
            return "world-insertion-failed";
        case GeneratedBodyCreateError::InvalidBodyId:
            return "invalid-body-id";
        case GeneratedBodyCreateError::InvalidMotion:
            return "invalid-motion";
        case GeneratedBodyCreateError::MaterialAssignmentFailed:
            return "material-assignment-failed";
        case GeneratedBodyCreateError::WorldIdentityMismatch:
            return "world-identity-mismatch";
        case GeneratedBodyCreateError::FinalSnapshotFailed:
            return "final-snapshot-failed";
        default:
            return "unknown";
        }
    }

    namespace
    {
        namespace HavokLayout = Addresses::Layouts::Havok;
        namespace BethesdaLayout = Addresses::Layouts::Bethesda;

        template <class Function, class... Arguments>
        [[nodiscard]] bool invokeVoid(const Function function, Arguments... arguments) noexcept
        {
            if (!function || !Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                function(arguments...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            function(arguments...);
            return true;
#endif
        }

        template <class Result, class Function, class... Arguments>
        [[nodiscard]] bool invokeResult(Result& result, const Function function, Arguments... arguments) noexcept
        {
            result = {};
            if (!function || !Memory::rangeHasAccess(
                    reinterpret_cast<const void*>(function), 1, Memory::Access::Execute)) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                result = function(arguments...);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                result = {};
                return false;
            }
#else
            result = function(arguments...);
            return true;
#endif
        }

        [[nodiscard]] bool zeroMemory(void* const memory, const std::size_t bytes) noexcept
        {
            if (!Memory::rangeHasAccess(memory, bytes, Memory::Access::Write)) {
                return false;
            }
#if defined(_MSC_VER)
            __try {
                std::memset(memory, 0, bytes);
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            std::memset(memory, 0, bytes);
            return true;
#endif
        }

        class BethesdaAllocatorContextGuard
        {
        public:
            BethesdaAllocatorContextGuard(const RuntimeModule& module, const std::uint32_t context) noexcept
            {
                if (!module) {
                    return;
                }
                std::uint32_t tlsIndex{};
                const auto indexAddress = module.resolve(Addresses::Symbol::Memory_BethesdaTlsIndex);
                if (indexAddress == 0 || !Memory::read(reinterpret_cast<const void*>(indexAddress), tlsIndex)) {
                    return;
                }
                const auto tlsBlock = reinterpret_cast<std::uintptr_t>(TlsGetValue(tlsIndex));
                if (tlsBlock == 0) {
                    return;
                }
                _slot = reinterpret_cast<std::uint32_t*>(tlsBlock + BethesdaLayout::ExeTls_AllocatorContext);
                if (!Memory::read(_slot, _previous) || !Memory::write(_slot, context)) {
                    _slot = nullptr;
                    return;
                }
                _active = true;
            }

            ~BethesdaAllocatorContextGuard() noexcept
            {
                if (_active && _slot) {
                    (void)Memory::write(_slot, _previous);
                }
            }

            BethesdaAllocatorContextGuard(const BethesdaAllocatorContextGuard&) = delete;
            BethesdaAllocatorContextGuard& operator=(const BethesdaAllocatorContextGuard&) = delete;

            [[nodiscard]] bool active() const noexcept { return _active; }

        private:
            std::uint32_t* _slot{};
            std::uint32_t _previous{};
            bool _active{};
        };

        [[nodiscard]] bool ensureBethesdaAllocator(const RuntimeModule& module) noexcept
        {
            const auto stateAddress = module.resolve(Addresses::Symbol::Memory_BethesdaAllocatorState);
            std::uint32_t state{};
            if (stateAddress == 0 || !Memory::read(reinterpret_cast<const void*>(stateAddress), state)) {
                return false;
            }
            if (state != BethesdaLayout::AllocatorReadyState) {
                using Function = void* (*)(void*, std::uint32_t*);
                const auto initialize = module.resolveFunction<Function>(Addresses::Symbol::Memory_BethesdaAllocatorInit);
                void* ignored{};
                if (!invokeResult(
                        ignored,
                        initialize,
                        reinterpret_cast<void*>(module.resolve(Addresses::Symbol::Memory_BethesdaAllocatorPool)),
                        reinterpret_cast<std::uint32_t*>(stateAddress)) ||
                    !Memory::read(reinterpret_cast<const void*>(stateAddress), state)) {
                    return false;
                }
            }
            return state == BethesdaLayout::AllocatorReadyState;
        }

        [[nodiscard]] void* allocateBethesda(const RuntimeModule& module, const std::size_t bytes) noexcept
        {
            if (bytes == 0 || !ensureBethesdaAllocator(module)) {
                return nullptr;
            }
            using Function = void* (*)(void*, std::size_t, std::uint32_t, char);
            const auto allocate = module.resolveFunction<Function>(Addresses::Symbol::Memory_BethesdaAllocate);
            void* result{};
            return invokeResult(
                       result,
                       allocate,
                       reinterpret_cast<void*>(module.resolve(Addresses::Symbol::Memory_BethesdaAllocatorPool)),
                       bytes,
                       std::uint32_t{},
                       char{}) ?
                result :
                nullptr;
        }

        [[nodiscard]] void* appendHavokArray(
            const HavokAllocator& allocator,
            void* const arrayHeader,
            const int elementBytes) noexcept
        {
            if (!arrayHeader || elementBytes <= 0) {
                return nullptr;
            }
            const auto header = reinterpret_cast<std::uintptr_t>(arrayHeader);
            void* data{};
            std::int32_t size{};
            std::int32_t capacityAndFlags{};
            if (!Memory::read(reinterpret_cast<const void*>(header + HavokLayout::Array_Data), data) ||
                !Memory::read(reinterpret_cast<const void*>(header + HavokLayout::Array_Size), size) ||
                !Memory::read(reinterpret_cast<const void*>(header + HavokLayout::Array_CapacityAndFlags), capacityAndFlags) || size < 0) {
                return nullptr;
            }

            auto capacity = static_cast<std::uint32_t>(capacityAndFlags) & HavokLayout::Array_CapacityMask;
            if (static_cast<std::uint32_t>(size) >= capacity) {
                if (!allocator.reserveArray(arrayHeader, elementBytes) ||
                    !Memory::read(reinterpret_cast<const void*>(header + HavokLayout::Array_Data), data) ||
                    !Memory::read(
                        reinterpret_cast<const void*>(header + HavokLayout::Array_CapacityAndFlags), capacityAndFlags)) {
                    return nullptr;
                }
                capacity = static_cast<std::uint32_t>(capacityAndFlags) & HavokLayout::Array_CapacityMask;
                if (static_cast<std::uint32_t>(size) >= capacity) {
                    return nullptr;
                }
            }
            if (!data || static_cast<std::size_t>(size) >
                    (std::numeric_limits<std::size_t>::max)() / static_cast<std::size_t>(elementBytes)) {
                return nullptr;
            }
            const auto entryAddress = reinterpret_cast<std::uintptr_t>(data) +
                                      static_cast<std::size_t>(size) * static_cast<std::size_t>(elementBytes);
            const auto nextSize = size + 1;
            if (!Memory::rangeHasAccess(
                    reinterpret_cast<void*>(entryAddress), static_cast<std::size_t>(elementBytes), Memory::Access::Write) ||
                !Memory::write(reinterpret_cast<void*>(header + HavokLayout::Array_Size), nextSize)) {
                return nullptr;
            }
            return reinterpret_cast<void*>(entryAddress);
        }

        [[nodiscard]] void* nativePhysicsSystemInstance(void* const physicsSystem) noexcept
        {
            void* instance{};
            return physicsSystem && Memory::read(
                       reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(physicsSystem) +
                                                     BethesdaLayout::PhysicsSystem_Instance),
                       instance) ?
                instance :
                nullptr;
        }

        [[nodiscard]] void* nativeWorldFromPhysicsSystem(void* const physicsSystem) noexcept
        {
            void* world{};
            void* const instance = nativePhysicsSystemInstance(physicsSystem);
            return instance && Memory::read(
                       reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(instance) +
                                                     BethesdaLayout::PhysicsSystemInstance_World),
                       world) ?
                world :
                nullptr;
        }

        [[nodiscard]] bool removePhysicsSystem(
            const RuntimeModule& module,
            void* const bhkWorld,
            void* const physicsSystem) noexcept
        {
            void* const instance = nativePhysicsSystemInstance(physicsSystem);
            if (!bhkWorld || !instance) {
                return false;
            }
            using Function = void (*)(void*, void*);
            return invokeVoid(
                module.resolveFunction<Function>(Addresses::Symbol::World_RemovePhysicsSystemInstance), bhkWorld, instance);
        }

        void detachAndReleaseNode(void* const collisionObject, void*& node) noexcept
        {
            if (!node) {
                return;
            }
            if (collisionObject) {
                void* nullOwner{};
                (void)Memory::write(
                    reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(collisionObject) +
                                            BethesdaLayout::CollisionObject_OwnerNode),
                    nullOwner);
            }

            const auto collisionSlot = reinterpret_cast<void*>(
                reinterpret_cast<std::uintptr_t>(node) + BethesdaLayout::NiAvObject_CollisionObject);
            void* nodeCollisionObject{};
            (void)Memory::read(collisionSlot, nodeCollisionObject);
            void* nullCollision{};
            (void)Memory::write(collisionSlot, nullCollision);
            if (nodeCollisionObject) {
                (void)releaseBethesdaReference(nodeCollisionObject);
            }
            (void)releaseBethesdaReference(node);
            node = nullptr;
        }

        [[nodiscard]] bool createAndLinkOwnerNode(
            const RuntimeModule& module,
            void* const collisionObject,
            const char* const name,
            void*& node) noexcept
        {
            node = nullptr;
            void* const storage = allocateBethesda(module, BethesdaLayout::NiNodeSize);
            if (!storage || !zeroMemory(storage, BethesdaLayout::NiNodeSize)) {
                return false;
            }
            using NodeConstructor = void* (*)(void*, std::uint16_t);
            void* constructed{};
            if (!invokeResult(
                    constructed,
                    module.resolveFunction<NodeConstructor>(Addresses::Symbol::Scene_NiNodeCtor),
                    storage,
                    std::uint16_t{}) ||
                !constructed) {
                return false;
            }
            node = constructed;

            if (name && *name) {
                alignas(8) std::array<std::byte, sizeof(void*)> fixedString{};
                using CreateString = void (*)(void*, const char*);
                using SetName = void (*)(void*, const void*);
                if (!invokeVoid(
                        module.resolveFunction<CreateString>(Addresses::Symbol::Scene_BSFixedStringCreate),
                        fixedString.data(),
                        name) ||
                    !invokeVoid(
                        module.resolveFunction<SetName>(Addresses::Symbol::Scene_NiNodeSetName),
                        node,
                        fixedString.data())) {
                    (void)releaseBethesdaReference(node);
                    node = nullptr;
                    return false;
                }
                fixedString.fill(std::byte{});
            }

            using Link = void (*)(void*, void*);
            if (!invokeVoid(
                    module.resolveFunction<Link>(Addresses::Symbol::Body_CollisionObjectLinkObject), collisionObject, node)) {
                (void)releaseBethesdaReference(node);
                node = nullptr;
                return false;
            }

            void* observedOwner{};
            void* observedCollision{};
            return Memory::read(
                       reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(collisionObject) +
                                                     BethesdaLayout::CollisionObject_OwnerNode),
                       observedOwner) &&
                   Memory::read(
                       reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(node) +
                                                     BethesdaLayout::NiAvObject_CollisionObject),
                       observedCollision) &&
                   observedOwner == node && observedCollision == collisionObject;
        }

        [[nodiscard]] std::uint16_t initialQuality(const GeneratedMotionType motionType) noexcept
        {
            switch (motionType) {
            case GeneratedMotionType::Static:
                return 0;
            case GeneratedMotionType::Dynamic:
                return 1;
            case GeneratedMotionType::Keyframed:
                return (std::numeric_limits<std::uint8_t>::max)();
            default:
                return (std::numeric_limits<std::uint8_t>::max)();
            }
        }

        [[nodiscard]] bool nonStaticMotionValid(const BodySnapshot& snapshot) noexcept
        {
            return snapshot.valid && snapshot.body.motionIndex > 0 &&
                   snapshot.body.motionIndex < HavokLayout::MaxUsableMotionIndex && snapshot.motion.valid;
        }

        struct CreateResources
        {
            const RuntimeModule* module{};
            void* systemData{};
            bool ownsSystemDataReference{};
            void* physicsSystem{};
            void* collisionObject{};
            void* node{};
            void* bhkWorld{};
            bool addedToWorld{};

            void cleanup() noexcept
            {
                if (addedToWorld && module) {
                    (void)removePhysicsSystem(*module, bhkWorld, physicsSystem);
                }
                detachAndReleaseNode(collisionObject, node);
                if (collisionObject) {
                    (void)releaseBethesdaReference(collisionObject);
                    collisionObject = nullptr;
                    physicsSystem = nullptr;
                } else if (physicsSystem) {
                    (void)releaseBethesdaReference(physicsSystem);
                    physicsSystem = nullptr;
                }
                if (ownsSystemDataReference && systemData) {
                    (void)releaseHavokReference(systemData);
                    systemData = nullptr;
                }
            }
        };
    }

    namespace Detail
    {
        class GeneratedBodyState
        {
        public:
            explicit GeneratedBodyState(const RuntimeModule module) noexcept : _module(module), _ownerThreadId(GetCurrentThreadId()) {}

            ~GeneratedBodyState() noexcept
            {
                for (auto& retirement : _retirements) {
                    if (retirement.occupied()) {
                        (void)retirement.nameStorage.release();
                    }
                }
            }

            [[nodiscard]] bool ready() const noexcept { return static_cast<bool>(_module); }
            [[nodiscard]] bool onOwnerThread() const noexcept { return GetCurrentThreadId() == _ownerThreadId; }

            [[nodiscard]] GeneratedBody create(
                const GeneratedBodyCreateInfo& info,
                GeneratedBodyCreateError& error,
                const std::shared_ptr<GeneratedBodyState>& self) noexcept
            {
                error = GeneratedBodyCreateError::None;
                GeneratedBody result{};
                if (!_module) {
                    error = GeneratedBodyCreateError::InvalidRuntime;
                    return result;
                }
                if (!onOwnerThread()) {
                    error = GeneratedBodyCreateError::WrongThread;
                    return result;
                }
                if (!info.hknpWorld || !info.bhkWorld || !info.shape || info.name.size() > GeneratedBodyMaximumNameBytes ||
                    info.materialId == (std::numeric_limits<std::uint16_t>::max)()) {
                    error = GeneratedBodyCreateError::InvalidInput;
                    return result;
                }

                try {
                    result._nameStorage = std::make_unique<std::string>(info.name.empty() ? "RPS_Body" : info.name);
                } catch (...) {
                    error = GeneratedBodyCreateError::NameAllocationFailed;
                    return result;
                }

                CreateResources resources{ .module = &_module, .bhkWorld = info.bhkWorld };
                HavokAllocator havokAllocator{ _module };
                resources.systemData = havokAllocator.allocate(BethesdaLayout::PhysicsSystemDataSize);
                if (!resources.systemData || !zeroMemory(resources.systemData, BethesdaLayout::PhysicsSystemDataSize)) {
                    error = GeneratedBodyCreateError::HavokAllocationFailed;
                    if (resources.systemData) {
                        (void)havokAllocator.deallocate(resources.systemData, BethesdaLayout::PhysicsSystemDataSize);
                    }
                    return result;
                }

                using UnaryConstructor = void* (*)(void*);
                void* constructedSystemData{};
                if (!invokeResult(
                        constructedSystemData,
                        _module.resolveFunction<UnaryConstructor>(Addresses::Symbol::Body_PhysicsSystemDataCtor),
                        resources.systemData) ||
                    constructedSystemData != resources.systemData) {
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    (void)havokAllocator.deallocate(resources.systemData, BethesdaLayout::PhysicsSystemDataSize);
                    resources.systemData = nullptr;
                    return result;
                }
                resources.systemData = constructedSystemData;
                resources.ownsSystemDataReference = true;

                std::uint32_t localMotionIndex = InvalidBodyId;
                if (info.motionType != GeneratedMotionType::Static) {
                    const auto motionArray = reinterpret_cast<void*>(
                        reinterpret_cast<std::uintptr_t>(resources.systemData) +
                        BethesdaLayout::PhysicsSystemData_MotionCinfos);
                    std::int32_t motionCount{};
                    if (!Memory::read(
                            reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(motionArray) + HavokLayout::Array_Size),
                            motionCount) ||
                        motionCount < 0) {
                        error = GeneratedBodyCreateError::NativeConstructionFailed;
                        resources.cleanup();
                        return result;
                    }
                    void* const motionCinfo = appendHavokArray(
                        havokAllocator, motionArray, static_cast<int>(BethesdaLayout::MotionCinfoSize));
                    void* constructedMotion{};
                    if (!motionCinfo || !invokeResult(
                            constructedMotion,
                            _module.resolveFunction<UnaryConstructor>(Addresses::Symbol::Body_MotionCinfoCtor),
                            motionCinfo) ||
                        !constructedMotion) {
                        error = GeneratedBodyCreateError::NativeConstructionFailed;
                        resources.cleanup();
                        return result;
                    }
                    localMotionIndex = static_cast<std::uint32_t>(motionCount);
                }

                void* const bodyCinfo = appendHavokArray(
                    havokAllocator,
                    reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(resources.systemData) +
                                            BethesdaLayout::PhysicsSystemData_BodyCinfos),
                    static_cast<int>(BethesdaLayout::BodyCinfoSize));
                void* constructedBodyCinfo{};
                if (!bodyCinfo || !invokeResult(
                        constructedBodyCinfo,
                        _module.resolveFunction<UnaryConstructor>(Addresses::Symbol::Body_BodyCinfoCtor),
                        bodyCinfo) ||
                    !constructedBodyCinfo) {
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    resources.cleanup();
                    return result;
                }

                const auto bodyCinfoAddress = reinterpret_cast<std::uintptr_t>(bodyCinfo);
                const void* shape = info.shape;
                const std::uint32_t invalidId = InvalidBodyId;
                const auto quality = initialQuality(info.motionType);
                const std::uint16_t localMaterial = BethesdaLayout::GeneratedLocalMaterialIndex;
                const char* const name = result._nameStorage->c_str();
                const std::uintptr_t userData{};
                if (!Memory::write(reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_Shape), shape) ||
                    !Memory::write(
                        reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_ReservedBodyId), invalidId) ||
                    !Memory::write(
                        reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_LocalMotionIndex), localMotionIndex) ||
                    !Memory::write(
                        reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_QualityId), quality) ||
                    !Memory::write(
                        reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_LocalMaterialIndex), localMaterial) ||
                    !Memory::write(
                        reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_CollisionFilterInfo),
                        info.collisionFilterInfo) ||
                    !Memory::write(reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_Name), name) ||
                    !Memory::write(reinterpret_cast<void*>(bodyCinfoAddress + BethesdaLayout::BodyCinfo_UserData), userData)) {
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    resources.cleanup();
                    return result;
                }

                void* const material = appendHavokArray(
                    havokAllocator,
                    reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(resources.systemData) +
                                            BethesdaLayout::PhysicsSystemData_Materials),
                    static_cast<int>(BethesdaLayout::MaterialSize));
                void* constructedMaterial{};
                if (!material || !invokeResult(
                        constructedMaterial,
                        _module.resolveFunction<UnaryConstructor>(Addresses::Symbol::Body_MaterialCtor),
                        material) ||
                    !constructedMaterial) {
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    resources.cleanup();
                    return result;
                }

                void* const shapeSlot = appendHavokArray(
                    havokAllocator,
                    reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(resources.systemData) +
                                            BethesdaLayout::PhysicsSystemData_Shapes),
                    static_cast<int>(BethesdaLayout::ShapeReferenceSize));
                if (!shapeSlot || !Memory::write(shapeSlot, shape)) {
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    resources.cleanup();
                    return result;
                }
                if (!addHavokReference(shape)) {
                    const void* nullShape{};
                    (void)Memory::write(shapeSlot, nullShape);
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    resources.cleanup();
                    return result;
                }

                void* const physicsSystemStorage = allocateBethesda(_module, BethesdaLayout::PhysicsSystemSize);
                if (!physicsSystemStorage) {
                    error = GeneratedBodyCreateError::BethesdaAllocationFailed;
                    resources.cleanup();
                    return result;
                }
                using PhysicsSystemConstructor = void* (*)(void*, void*);
                if (!invokeResult(
                        resources.physicsSystem,
                        _module.resolveFunction<PhysicsSystemConstructor>(Addresses::Symbol::Body_PhysicsSystemCtor),
                        physicsSystemStorage,
                        resources.systemData) ||
                    !resources.physicsSystem) {
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    resources.cleanup();
                    return result;
                }
                (void)releaseHavokReference(resources.systemData);
                resources.ownsSystemDataReference = false;

                {
                    BethesdaAllocatorContextGuard allocatorContext{
                        _module, BethesdaLayout::CollisionObjectAllocatorContext
                    };
                    if (!allocatorContext.active()) {
                        error = GeneratedBodyCreateError::BethesdaAllocationFailed;
                        resources.cleanup();
                        return result;
                    }
                    void* const collisionStorage = allocateBethesda(_module, BethesdaLayout::CollisionObjectSize);
                    using CollisionConstructor = void* (*)(void*, std::uint32_t, void*);
                    if (!collisionStorage || !invokeResult(
                            resources.collisionObject,
                            _module.resolveFunction<CollisionConstructor>(Addresses::Symbol::Body_CollisionObjectCtor),
                            collisionStorage,
                            std::uint32_t{},
                            resources.physicsSystem) ||
                        !resources.collisionObject) {
                        error = GeneratedBodyCreateError::NativeConstructionFailed;
                        resources.cleanup();
                        return result;
                    }
                }

                if (!createAndLinkOwnerNode(
                        _module, resources.collisionObject, result._nameStorage->c_str(), resources.node)) {
                    error = GeneratedBodyCreateError::SceneOwnerCreationFailed;
                    resources.cleanup();
                    return result;
                }

                using AddToWorld = void (*)(void*, void*);
                if (!invokeVoid(
                        _module.resolveFunction<AddToWorld>(Addresses::Symbol::Body_CollisionObjectAddToWorld),
                        resources.collisionObject,
                        info.bhkWorld)) {
                    error = GeneratedBodyCreateError::WorldInsertionFailed;
                    resources.cleanup();
                    return result;
                }
                resources.addedToWorld = nativePhysicsSystemInstance(resources.physicsSystem) != nullptr;
                if (!resources.addedToWorld) {
                    error = GeneratedBodyCreateError::WorldInsertionFailed;
                    resources.cleanup();
                    return result;
                }

                using GetBodyId = void (*)(void*, BodyId*, std::int32_t);
                BodyId bodyId{};
                if (!invokeVoid(
                        _module.resolveFunction<GetBodyId>(Addresses::Symbol::Body_PhysicsSystemGetBodyId),
                        resources.physicsSystem,
                        &bodyId,
                        std::int32_t{}) ||
                    !bodyId.valid()) {
                    error = GeneratedBodyCreateError::InvalidBodyId;
                    resources.cleanup();
                    return result;
                }

                const auto initialSnapshot = snapshotBodyDuringSafeEpoch(info.hknpWorld, bodyId);
                if (!initialSnapshot.valid ||
                    (info.motionType != GeneratedMotionType::Static && !nonStaticMotionValid(initialSnapshot))) {
                    error = GeneratedBodyCreateError::InvalidMotion;
                    resources.cleanup();
                    return result;
                }

                using SetMaterial = void (*)(void*, std::uint32_t, std::uint16_t, std::int32_t);
                if (!invokeVoid(
                        _module.resolveFunction<SetMaterial>(Addresses::Symbol::Physics_SetBodyMaterial),
                        info.hknpWorld,
                        bodyId.value,
                        info.materialId,
                        std::int32_t{})) {
                    error = GeneratedBodyCreateError::MaterialAssignmentFailed;
                    resources.cleanup();
                    return result;
                }
                auto snapshot = snapshotBodyDuringSafeEpoch(info.hknpWorld, bodyId);
                if (!snapshot.valid || snapshot.body.materialId != info.materialId) {
                    error = GeneratedBodyCreateError::MaterialAssignmentFailed;
                    resources.cleanup();
                    return result;
                }

                if (info.motionType == GeneratedMotionType::Keyframed) {
                    using SetKeyframed = void (*)(void*, std::uint32_t);
                    if (!invokeVoid(
                            _module.resolveFunction<SetKeyframed>(Addresses::Symbol::Physics_SetBodyKeyframed),
                            info.hknpWorld,
                            bodyId.value)) {
                        error = GeneratedBodyCreateError::NativeConstructionFailed;
                        resources.cleanup();
                        return result;
                    }
                } else {
                    using SetMotionType = void (*)(void*, int);
                    if (!invokeVoid(
                            _module.resolveFunction<SetMotionType>(Addresses::Symbol::Body_CollisionObjectSetMotionType),
                            resources.collisionObject,
                            info.motionType == GeneratedMotionType::Static ? 0 : 1)) {
                        error = GeneratedBodyCreateError::NativeConstructionFailed;
                        resources.cleanup();
                        return result;
                    }
                }

                using SetFilter = void (*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
                using SetFlags = void (*)(void*, std::uint32_t, std::uint32_t, std::uint32_t);
                using Activate = void (*)(void*, std::uint32_t);
                if (!invokeVoid(
                        _module.resolveFunction<SetFilter>(Addresses::Symbol::Physics_SetBodyCollisionFilterInfo),
                        info.hknpWorld,
                        bodyId.value,
                        info.collisionFilterInfo,
                        std::uint32_t{ 1 }) ||
                    !invokeVoid(
                        _module.resolveFunction<SetFlags>(Addresses::Symbol::Physics_EnableBodyFlags),
                        info.hknpWorld,
                        bodyId.value,
                        BethesdaLayout::GeneratedBodyRuntimeFlags,
                        BethesdaLayout::RebuildBodyCollisionState) ||
                    !invokeVoid(
                        _module.resolveFunction<Activate>(Addresses::Symbol::Physics_ActivateBody),
                        info.hknpWorld,
                        bodyId.value)) {
                    error = GeneratedBodyCreateError::NativeConstructionFailed;
                    resources.cleanup();
                    return result;
                }

                void* wrapperWorld{};
                (void)Memory::read(
                    reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(info.bhkWorld) +
                                                  HavokLayout::BhkWorld_HknpWorld),
                    wrapperWorld);
                if (wrapperWorld != info.hknpWorld || nativeWorldFromPhysicsSystem(resources.physicsSystem) != info.hknpWorld) {
                    error = GeneratedBodyCreateError::WorldIdentityMismatch;
                    resources.cleanup();
                    return result;
                }

                snapshot = snapshotBodyDuringSafeEpoch(info.hknpWorld, bodyId);
                if (!snapshot.valid || snapshot.body.collisionFilterInfo != info.collisionFilterInfo ||
                    snapshot.body.userDataAddress != reinterpret_cast<std::uintptr_t>(resources.collisionObject)) {
                    error = GeneratedBodyCreateError::FinalSnapshotFailed;
                    resources.cleanup();
                    return result;
                }

                result._state = self;
                result._collisionObject = resources.collisionObject;
                result._physicsSystem = resources.physicsSystem;
                result._niNode = resources.node;
                result._hknpWorld = info.hknpWorld;
                result._bhkWorld = info.bhkWorld;
                result._bodyId = bodyId;
                resources.collisionObject = nullptr;
                resources.physicsSystem = nullptr;
                resources.node = nullptr;
                resources.addedToWorld = false;
                error = GeneratedBodyCreateError::None;
                return result;
            }

            [[nodiscard]] bool retire(GeneratedBody& body, const bool requireOwnerThread) noexcept
            {
                if (!body.valid() || body._state.get() != this || (requireOwnerThread && !onOwnerThread())) {
                    return false;
                }

                std::scoped_lock lock(_mutex);
                auto slot = std::find_if(_retirements.begin(), _retirements.end(), [](const Retirement& value) {
                    return !value.occupied();
                });
                if (slot == _retirements.end()) {
                    if (onOwnerThread()) {
                        (void)removePhysicsSystem(_module, body._bhkWorld, body._physicsSystem);
                    }
                    (void)body._nameStorage.release();
                    body.clearWithoutRelease();
                    return true;
                }

                slot->collisionObject = body._collisionObject;
                slot->physicsSystem = body._physicsSystem;
                slot->node = body._niNode;
                slot->bhkWorld = body._bhkWorld;
                slot->bodyId = body._bodyId;
                slot->nameStorage = std::move(body._nameStorage);
                slot->phase = RetirementPhase::PendingRemoval;
                slot->remainingSteps = GeneratedBodyRetirementGraceSteps;
                ++_retirementCount;
                body.clearWithoutRelease();

                if (onOwnerThread() && removePhysicsSystem(_module, slot->bhkWorld, slot->physicsSystem)) {
                    slot->phase = RetirementPhase::GraceWindow;
                }
                return true;
            }

            [[nodiscard]] std::size_t serviceOwnerThreadRetirements() noexcept
            {
                if (!onOwnerThread()) {
                    return 0;
                }
                std::scoped_lock lock(_mutex);
                std::size_t removed{};
                for (auto& retirement : _retirements) {
                    if (retirement.phase == RetirementPhase::PendingRemoval &&
                        removePhysicsSystem(_module, retirement.bhkWorld, retirement.physicsSystem)) {
                        retirement.phase = RetirementPhase::GraceWindow;
                        ++removed;
                    }
                }
                return removed;
            }

            [[nodiscard]] std::size_t serviceCompletedPhysicsSteps(const std::uint32_t steps) noexcept
            {
                if (steps == 0) {
                    return 0;
                }
                std::scoped_lock lock(_mutex);
                std::size_t released{};
                for (auto& retirement : _retirements) {
                    if (retirement.phase != RetirementPhase::GraceWindow) {
                        continue;
                    }
                    retirement.remainingSteps = advanceGeneratedBodyRetirement(retirement.remainingSteps, steps);
                    if (retirement.remainingSteps != 0) {
                        continue;
                    }
                    release(retirement);
                    ++released;
                }
                return released;
            }

            [[nodiscard]] std::size_t shutdownAfterWorldLoss() noexcept
            {
                if (!onOwnerThread()) {
                    return 0;
                }
                std::scoped_lock lock(_mutex);
                std::size_t released{};
                for (auto& retirement : _retirements) {
                    if (!retirement.occupied()) {
                        continue;
                    }
                    release(retirement);
                    ++released;
                }
                return released;
            }

            [[nodiscard]] std::size_t retirementCount() const noexcept
            {
                std::scoped_lock lock(_mutex);
                return _retirementCount;
            }

        private:
            enum class RetirementPhase : std::uint8_t
            {
                Empty,
                PendingRemoval,
                GraceWindow,
            };

            struct Retirement
            {
                std::unique_ptr<std::string> nameStorage{};
                void* collisionObject{};
                void* physicsSystem{};
                void* node{};
                void* bhkWorld{};
                BodyId bodyId{};
                std::uint32_t remainingSteps{};
                RetirementPhase phase{ RetirementPhase::Empty };

                [[nodiscard]] bool occupied() const noexcept { return phase != RetirementPhase::Empty; }
            };

            void release(Retirement& retirement) noexcept
            {
                detachAndReleaseNode(retirement.collisionObject, retirement.node);
                if (retirement.collisionObject) {
                    (void)releaseBethesdaReference(retirement.collisionObject);
                }
                retirement = {};
                if (_retirementCount > 0) {
                    --_retirementCount;
                }
            }

            RuntimeModule _module{};
            DWORD _ownerThreadId{};
            mutable std::mutex _mutex{};
            std::array<Retirement, GeneratedBodyRetirementCapacity> _retirements{};
            std::size_t _retirementCount{};
        };
    }

    GeneratedBody::~GeneratedBody() noexcept
    {
        retireOrQueue();
    }

    GeneratedBody::GeneratedBody(GeneratedBody&& other) noexcept
    {
        moveFrom(std::move(other));
    }

    GeneratedBody& GeneratedBody::operator=(GeneratedBody&& other) noexcept
    {
        if (this != std::addressof(other)) {
            retireOrQueue();
            moveFrom(std::move(other));
        }
        return *this;
    }

    void GeneratedBody::retireOrQueue() noexcept
    {
        if (valid() && _state) {
            auto state = _state;
            (void)state->retire(*this, false);
        }
    }

    void GeneratedBody::clearWithoutRelease() noexcept
    {
        _state.reset();
        _nameStorage.reset();
        _collisionObject = nullptr;
        _physicsSystem = nullptr;
        _niNode = nullptr;
        _hknpWorld = nullptr;
        _bhkWorld = nullptr;
        _bodyId = {};
    }

    void GeneratedBody::moveFrom(GeneratedBody&& other) noexcept
    {
        _state = std::move(other._state);
        _nameStorage = std::move(other._nameStorage);
        _collisionObject = std::exchange(other._collisionObject, nullptr);
        _physicsSystem = std::exchange(other._physicsSystem, nullptr);
        _niNode = std::exchange(other._niNode, nullptr);
        _hknpWorld = std::exchange(other._hknpWorld, nullptr);
        _bhkWorld = std::exchange(other._bhkWorld, nullptr);
        _bodyId = std::exchange(other._bodyId, BodyId{});
    }

    GeneratedBodyService::GeneratedBodyService(const RuntimeModule module) noexcept
    {
        try {
            _state = std::make_shared<Detail::GeneratedBodyState>(module);
        } catch (...) {
            _state.reset();
        }
    }

    GeneratedBody GeneratedBodyService::create(
        const GeneratedBodyCreateInfo& info,
        GeneratedBodyCreateError& error) noexcept
    {
        if (!_state) {
            error = GeneratedBodyCreateError::InvalidRuntime;
            return {};
        }
        return _state->create(info, error, _state);
    }

    bool GeneratedBodyService::ready() const noexcept
    {
        return _state && _state->ready();
    }

    bool GeneratedBodyService::retire(GeneratedBody& body) noexcept
    {
        return _state && _state->retire(body, true);
    }

    std::size_t GeneratedBodyService::serviceOwnerThreadRetirements() noexcept
    {
        return _state ? _state->serviceOwnerThreadRetirements() : 0;
    }

    std::size_t GeneratedBodyService::serviceCompletedPhysicsSteps(const std::uint32_t completedPhysicsSteps) noexcept
    {
        return _state ? _state->serviceCompletedPhysicsSteps(completedPhysicsSteps) : 0;
    }

    std::size_t GeneratedBodyService::shutdownAfterWorldLoss() noexcept
    {
        return _state ? _state->shutdownAfterWorldLoss() : 0;
    }

    std::size_t GeneratedBodyService::retirementCount() const noexcept
    {
        return _state ? _state->retirementCount() : 0;
    }
}
