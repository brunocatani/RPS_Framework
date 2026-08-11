#include "RPS/Addresses/Catalog.h"
#include "RPS/Addresses/Layouts.h"
#include "RPS/Runtime/Audio.h"

#include <iostream>

int main()
{
    using namespace RPS;
    using namespace RPS::Runtime;
    using namespace RPS::Runtime::Audio;

    NativeSoundHandle handle{};
    if (handle.active() || handle.soundId() != Addresses::Layouts::Audio::InvalidSoundId ||
        handle.assumesSuccess() || handle.state() != 0) {
        std::cerr << "native sound handle did not initialize invalid\n";
        return 1;
    }
    NativeSoundHandle moved{ static_cast<NativeSoundHandle&&>(handle) };
    if (handle.active() || moved.active()) {
        std::cerr << "native sound handle move did not preserve invalid ownership\n";
        return 1;
    }

    if (Addresses::record(Addresses::Symbol::Audio_PlayFollowingDescriptor).rva != 0x02C7D70 ||
        Addresses::record(Addresses::Symbol::Audio_FadeOutAndRelease).rva != 0x1B4B3E0 ||
        Addresses::record(Addresses::Symbol::Audio_ManagerSingleton).rva != 0x5B6DB90) {
        std::cerr << "native audio catalog mismatch\n";
        return 1;
    }

    const auto module = RuntimeModule::detect();
    AudioApi api{ module };
    Scene::Point3 position{};
    int descriptor{};
    int node{};
    if (module || api.playFollowingDescriptor(moved, &descriptor, &node, position).status !=
                      AudioStatus::InvalidRuntime ||
        api.setVolume(moved, 0.5f).status != AudioStatus::InvalidRuntime ||
        api.fadeInPlay(moved, 100).status != AudioStatus::InvalidRuntime ||
        api.fadeOutAndRelease(moved, 100).status != AudioStatus::InvalidRuntime ||
        toString(AudioStatus::HandleAlreadyActive) != "handle-already-active") {
        std::cerr << "audio API did not fail closed without FO4VR\n";
        return 1;
    }

    return 0;
}
