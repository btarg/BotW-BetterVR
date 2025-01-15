#include "../instance.h"
#include "cemu_hooks.h"

#include <glm/gtx/quaternion.hpp>

#define PADDED_BYTES(from, up) uint8_t byte_##from##[ ## (up-from+0x04) ## ]


#pragma pack(push, 1)
namespace sead {
    struct SafeString {
        BEType<uint32_t> c_str;
        BEType<uint32_t> vtable;
    };

    struct PtrArrayImpl {
        BEType<uint32_t> size;
        BEType<uint32_t> capacity;
        BEType<uint32_t> data;
    };
};

struct ActorPhysics {
    BEType<uint32_t> __vftable;
    sead::SafeString actorName;
    BEType<uint32_t> *physicsParamSet;
    BEType<uint32_t> flags;
    BEType<uint32_t> dword14;
    BEType<uint32_t> dword18;
    BEType<float> scale;
    BEType<uint32_t> gsysModel;
    sead::PtrArrayImpl rigidBodies;
    sead::PtrArrayImpl collisionInfo;
    sead::PtrArrayImpl contactInfo;
};

struct ActorWiiU {
    uint32_t vtable;
    BEType<uint32_t> baseProcPtr;
    uint8_t unk_08[0xF4 - 0x08];
    uint32_t physicsMainBodyPtr; // 0xF4
    uint32_t physicsTgtBodyPtr; // 0xF8
    uint8_t unk_FC[0x1F8 - 0xFC];
    BEMatrix34 mtx;
    uint32_t physicsMtxPtr;
    BEMatrix34 homeMtx;
    BEVec3 velocity;
    BEVec3 angularVelocity;
    BEVec3 scale; // 0x274
    BEType<float> dispDistSq;
    BEType<float> deleteDistSq;
    BEType<float> loadDistP10;
    BEVec3 previousPos;
    BEVec3 previousPos2;
    PADDED_BYTES(0x2A4, 0x324);
    BEType<uint32_t> modelBindInfoPtr;
    PADDED_BYTES(0x32C, 0x32C);
    BEType<uint32_t> gsysModelPtr;
    PADDED_BYTES(0x334, 0x334);
    BEType<float> startModelOpacity;
    BEType<float> modelOpacity;
    PADDED_BYTES(0x340, 0x348);
    struct {
        BEType<float> minX;
        BEType<float> minY;
        BEType<float> minZ;
        BEType<float> maxX;
        BEType<float> maxY;
        BEType<float> maxZ;
    } aabb;
    BEType<uint32_t> flags2;
    BEType<uint32_t> flags2Copy;
    BEType<uint32_t> flags;
    BEType<uint32_t> flags3; // 0x370 or 880. However in IDA there's a 0xF4 offset

    PADDED_BYTES(0x374, 0x39C);
    BEType<uint32_t> actorPhysicsPtr; // 0x3A0
    PADDED_BYTES(0x3A4, 0x404);

    BEType<uint32_t> hashId;
    PADDED_BYTES(0x40C, 0x430);
    uint8_t unk_434;
    uint8_t unk_435;
    uint8_t opacityOrSomethingEnabled;
    uint8_t unk_437;
    PADDED_BYTES(0x438, 0x440);
    BEType<uint32_t> actorX6A0Ptr;
    BEType<uint32_t> chemicalsPtr;
    BEType<uint32_t> reactionsPtr;
    PADDED_BYTES(0x450, 0x48C);
    BEType<float> lodDrawDistanceMultiplier;
    PADDED_BYTES(0x494, 0x538);
};
#pragma pack(pop)

static_assert(sizeof(ActorWiiU) == 0x53C);




static uint32_t stringToHash(const char* str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash << 7) + *str++;
    }
    return hash;
}

std::mutex g_actorListMutex;
std::unordered_map<uint32_t, std::pair<std::string, uint32_t>> s_knownActors;

void CemuHooks::hook_UpdateActorList(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    std::scoped_lock lock(g_actorListMutex);

    // r7 holds actor list size
    // r5 holds current actor index
    // r6 holds current actor* list entry

    // clear actor list when reiterating actor list again
    if (hCPU->gpr[5] == 0) {
        s_knownActors.clear();
    }

    uint32_t actorLinkPtr = hCPU->gpr[6] + offsetof(ActorWiiU, baseProcPtr);
    uint32_t actorNamePtr = 0;
    readMemoryBE(actorLinkPtr, &actorNamePtr);
    if (actorNamePtr == 0)
        return;

    char* actorName = (char*)s_memoryBaseAddress + actorNamePtr;

    if (actorName[0] != '\0') {
        // Log::print("Updating actor list [{}/{}] {:08x} - {}", hCPU->gpr[5], hCPU->gpr[7], hCPU->gpr[6], actorName);
        uint32_t actorId = hCPU->gpr[6] + stringToHash(actorName);
        s_knownActors.emplace(actorId, std::make_pair(actorName, hCPU->gpr[6]));
    }

    // if (strcmp(actorName, "Weapon_Sword_056") == 0) {
    //     // Log::print("Updating actor list [{}/{}] {:08x} - {}", hCPU->gpr[5], hCPU->gpr[7], hCPU->gpr[6], actorName);
    //     // float velocityY = 0.0f;
    //     // readMemoryBE(hCPU->gpr[6] + offsetof(ActorWiiU, velocity.y), &velocityY);
    //     // velocityY = velocityY * 1.5f;
    //     // writeMemoryBE(hCPU->gpr[6] + offsetof(ActorWiiU, velocity.y), &velocityY);
    //     s_currActorPtrs.emplace_back(hCPU->gpr[6]);
    // }
}

// ksys::phys::RigidBodyFromShape::create to create a RigidBody from a shape
// use Actor::getRigidBodyByName

std::unordered_map<uint32_t, std::pair<std::string, uint32_t>> s_alreadyAddedActors;

void CemuHooks::updateFrames() {
    auto& overlay = VRManager::instance().VK->m_imguiOverlay;

    std::scoped_lock lock(g_actorListMutex);

    if (overlay) {
        // remove actors in s_alreadyAddedActors that are no longer in s_knownActors
        for (const auto& hash : s_alreadyAddedActors | std::views::keys) {
            if (!s_knownActors.contains(hash)) {
                overlay->RemoveEntity(hash);
            }
        }

        s_alreadyAddedActors = s_knownActors;

        // find current player (GameROMPlayer)
        BEMatrix34 playerPos = {};
        for (const auto& [actorId, actorData] : s_knownActors) {
            if (actorData.first == "GameROMPlayer") {
                readMemory(actorData.second + offsetof(ActorWiiU, mtx), &playerPos);
                glm::fvec3 newPlayerPos = playerPos.getPos().getLE();
                if (glm::distance(newPlayerPos, overlay->m_playerPos) > 25.0f) {
                    overlay->m_resetPlot = true;
                }
                overlay->m_playerPos = newPlayerPos;

                // // set invisibility flag
                // {
                //     BEType<int32_t> flags = 0;
                //     readMemory(actorData.second + offsetof(ActorWiiU, flags3), &flags);
                //     flags = flags.getLE() | 0x800;
                //     writeMemory(actorData.second + offsetof(ActorWiiU, flags3), &flags);
                // }
                // {
                //     BEType<int32_t> flags = 0;
                //     readMemory(actorData.second + offsetof(ActorWiiU, flags2), &flags);
                //     flags = flags.getLE() | 0x20;
                //     writeMemory(actorData.second + offsetof(ActorWiiU, flags2), &flags);
                //     writeMemory(actorData.second + offsetof(ActorWiiU, flags2Copy), &flags);
                // }
                // {
                //     float lodDrawDistanceMultiplier = 0;
                //     readMemory(actorData.second + offsetof(ActorWiiU, lodDrawDistanceMultiplier), &lodDrawDistanceMultiplier);
                //     lodDrawDistanceMultiplier = 0.0f;
                //     writeMemory(actorData.second + offsetof(ActorWiiU, lodDrawDistanceMultiplier), &lodDrawDistanceMultiplier);
                // }
                // {
                //     float startModelOpacity = 0;
                //     readMemory(actorData.second + offsetof(ActorWiiU, startModelOpacity), &startModelOpacity);
                //     startModelOpacity = 0.0f;
                //     writeMemory(actorData.second + offsetof(ActorWiiU, startModelOpacity), &startModelOpacity);
                // }
                // {
                //     BEType<float> modelOpacity = 1.0f;
                //     readMemory(actorData.second + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
                //     modelOpacity = 1.0f;
                //     writeMemory(actorData.second + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
                // }
                // {
                //     uint8_t opacityOrSomethingEnabled = 0;
                //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);
                //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled)+1, &opacityOrSomethingEnabled);
                //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled)-1, &opacityOrSomethingEnabled);
                //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled)-2, &opacityOrSomethingEnabled);
                // }
            }
            else if (actorData.first == "GameRomCamera") {
                readMemory(actorData.second + offsetof(ActorWiiU, mtx), &playerPos);
                glm::fvec3 newPlayerPos = playerPos.getPos().getLE();
            }
            else if (actorData.first.starts_with("Weapon_Sword")) {
                // BEType<float> modelOpacity = 1.0f;
                // writeMemory(actorData.second + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
                // uint8_t opacityOrSomethingEnabled = 1;
                // writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);
            }
        }

        // add actors that aren't in the overlay already
        for (auto& [actorId, actorInfo] : s_knownActors) {
            uint32_t actorPtr = actorInfo.second;
            const std::string& actorName = actorInfo.first;

            auto addField = [&]<typename T>(const std::string& name, uint32_t offset) -> void {
                uint32_t address = actorPtr + offset;
                overlay->AddOrUpdateEntity(actorId, actorName, name, address, getMemory<T>(address), true);
            };

            auto addMemoryRange = [&](const std::string& name, uint32_t addressPtr, uint32_t size) -> void {
                uint32_t address = 0;
                if (readMemoryBE(addressPtr, &address); address != 0) {
                    overlay->AddOrUpdateEntity(actorId, actorName, name, address, MemoryRange{ address, address + size, std::make_unique<MemoryEditor>() }, true);
                }
            };

            BEMatrix34 mtx = getMemory<BEMatrix34>(actorPtr + offsetof(ActorWiiU, mtx));
            overlay->AddOrUpdateEntity(actorId, actorName, "mtx", actorPtr + offsetof(ActorWiiU, mtx), mtx);
            if (playerPos.pos_x.getLE() != 0.0f) {
                overlay->SetPosition(actorId, playerPos.getPos(), mtx.getPos());
            }
            overlay->SetRotation(actorId, mtx.getRotLE());

            BEVec3 aabbMin = getMemory<BEVec3>(actorPtr + offsetof(ActorWiiU, aabb.minX));
            BEVec3 aabbMax = getMemory<BEVec3>(actorPtr + offsetof(ActorWiiU, aabb.maxX));
            if (aabbMin.x.getLE() != 0.0f) {
                overlay->SetAABB(actorId, aabbMin.getLE(), aabbMax.getLE());
            }

            // uint32_t physicsMtxPtr = 0;
            // if (readMemoryBE(actorPtr + offsetof(ActorWiiU, physicsMtxPtr), &physicsMtxPtr); physicsMtxPtr != 0) {
            //     overlay->AddOrUpdateEntity(actorId, actorName, "physicsMtx", physicsMtxPtr, getMemory<BEMatrix34>(physicsMtxPtr));
            // }
            addField.operator()<BEVec3>("velocity", offsetof(ActorWiiU, velocity));
            addField.operator()<BEVec3>("angularVelocity", offsetof(ActorWiiU, angularVelocity));
            addField.operator()<BEVec3>("scale", offsetof(ActorWiiU, scale));
            // addField.operator()<BEVec3>("previousPos", offsetof(ActorWiiU, previousPos));
            // addField.operator()<BEVec3>("previousPos2", offsetof(ActorWiiU, previousPos2));
            // addField.operator()<float>("dispDistSq", offsetof(ActorWiiU, dispDistSq));
            // addField.operator()<float>("deleteDistSq", offsetof(ActorWiiU, deleteDistSq));
            // addField.operator()<float>("loadDistP10", offsetof(ActorWiiU, loadDistP10));
            // addField.operator()<uint32_t>("modelBindInfoPtr", offsetof(ActorWiiU, modelBindInfoPtr));
            // addField.operator()<uint32_t>("gsysModelPtr", offsetof(ActorWiiU, gsysModelPtr));
            // addField.operator()<float>("startModelOpacity", offsetof(ActorWiiU, startModelOpacity));
            // addField.operator()<float>("modelOpacity", offsetof(ActorWiiU, modelOpacity));
            // addField.operator()<uint8_t>("opacityOrSomethingEnabled", offsetof(ActorWiiU, opacityOrSomethingEnabled));
            addField.operator()<BEVec3>("aabb_min", offsetof(ActorWiiU, aabb.minX));
            addField.operator()<BEVec3>("aabb_max", offsetof(ActorWiiU, aabb.maxX));
            addField.operator()<uint32_t>("flags2", offsetof(ActorWiiU, flags2));
            addField.operator()<uint32_t>("flags2Copy", offsetof(ActorWiiU, flags2Copy));
            addField.operator()<uint32_t>("flags", offsetof(ActorWiiU, flags));
            addField.operator()<uint32_t>("flags3", offsetof(ActorWiiU, flags3));

            // uint32_t actorPhysicsPtr = 0;
            // if (readMemoryBE(actorPtr + offsetof(ActorWiiU, actorPhysicsPtr), &actorPhysicsPtr); actorPhysicsPtr != 0) {
            //     float scale = 0.0f;
            //     readMemoryBE(actorPhysicsPtr + offsetof(ActorPhysics, scale), &scale);
            //     overlay->AddOrUpdateEntity(actorId, actorName, "scale", actorPhysicsPtr + offsetof(ActorPhysics, scale), BEType<float>(scale), true);
            //
            //     uint32_t rigidBodySize = 0;
            //     readMemoryBE(actorPhysicsPtr + offsetof(ActorPhysics, rigidBodies.size), &rigidBodySize);
            //     overlay->AddOrUpdateEntity(actorId, actorName, "rigidBodies.size", actorPhysicsPtr + offsetof(ActorPhysics, rigidBodies.size), BEType<uint32_t>(rigidBodySize), true);
            //
            //     uint32_t contactInfoSize = 0;
            //     readMemoryBE(actorPhysicsPtr + offsetof(ActorPhysics, contactInfo.size), &contactInfoSize);
            //     overlay->AddOrUpdateEntity(actorId, actorName, "contactInfo.size", actorPhysicsPtr + offsetof(ActorPhysics, contactInfo.size), BEType<uint32_t>(contactInfoSize), true);
            //
            //     uint32_t collisionInfoSize = 0;
            //     readMemoryBE(actorPhysicsPtr + offsetof(ActorPhysics, collisionInfo.size), &collisionInfoSize);
            //     overlay->AddOrUpdateEntity(actorId, actorName, "collisionInfo.size", actorPhysicsPtr + offsetof(ActorPhysics, collisionInfo.size), BEType<uint32_t>(collisionInfoSize), true);
            //
            //     uint32_t flags = 0;
            //     readMemoryBE(actorPhysicsPtr + offsetof(ActorPhysics, flags), &flags);
            //     overlay->AddOrUpdateEntity(actorId, actorName, "physicsFlags", actorPhysicsPtr + offsetof(ActorPhysics, flags), BEType<uint32_t>(flags), true);
            //
            //     auto AddFlagFromUint32 = [&](const std::string& name, uint32_t flag) {
            //         overlay->AddOrUpdateEntity(actorId, actorName, name, actorPhysicsPtr + offsetof(ActorPhysics, flags), BEType<uint8_t>((flags >> flag) & 0x1), false);
            //     };
            //
            //     AddFlagFromUint32("physicsFlag0", 0);
            //     AddFlagFromUint32("physicsFlag1", 1);
            //     AddFlagFromUint32("physicsFlag2", 2);
            //     AddFlagFromUint32("physicsFlag3", 3);
            //     AddFlagFromUint32("physicsFlag4", 4);
            //     AddFlagFromUint32("physicsFlag5", 5);
            //     AddFlagFromUint32("physicsFlag6", 6);
            //     AddFlagFromUint32("physicsFlag7", 7);
            //     AddFlagFromUint32("physicsFlag8", 8);
            //     AddFlagFromUint32("physicsFlag9", 9);
            //     AddFlagFromUint32("physicsFlag10", 10);
            //     AddFlagFromUint32("physicsFlag11", 11);
            //     AddFlagFromUint32("physicsFlag12", 12);
            //     AddFlagFromUint32("physicsFlag13", 13);
            //     AddFlagFromUint32("physicsFlag14", 14);
            //     AddFlagFromUint32("physicsFlag15", 15);
            //     AddFlagFromUint32("physicsFlag16", 16);
            //     AddFlagFromUint32("physicsFlag17", 17);
            //     AddFlagFromUint32("physicsFlag18", 18);
            //     AddFlagFromUint32("physicsFlag19", 19);
            //     AddFlagFromUint32("physicsFlag20", 20);
            //     AddFlagFromUint32("physicsFlag21", 21);
            //     AddFlagFromUint32("physicsFlag22", 22);
            //     AddFlagFromUint32("physicsFlag23", 23);
            //     AddFlagFromUint32("physicsFlag24", 24);
            //     AddFlagFromUint32("physicsFlag25", 25);
            //     AddFlagFromUint32("physicsFlag26", 26);
            //     AddFlagFromUint32("physicsFlag27", 27);
            //     AddFlagFromUint32("physicsFlag28", 28);
            //     AddFlagFromUint32("physicsFlag29", 29);
            //     AddFlagFromUint32("physicsFlag30", 30);
            //     AddFlagFromUint32("physicsFlag31", 31);
            // }


            {
                // show each bit of flags2
                // uint32_t flags2 = getMemory<int32_t>(actorPtr + offsetof(ActorWiiU, flags2)).getLE();
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_0", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>(flags2 & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_1", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 1) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_2", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 2) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_3", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 3) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_4", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 4) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_5", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 5) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_6", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 6) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_7", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 7) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_8", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 8) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_9", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 9) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_10", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 10) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_11", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 11) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_12", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 12) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_13", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 13) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_14", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 14) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_15", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 15) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_16", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 16) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_17", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 17) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_18", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 18) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_19", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 19) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_20", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 20) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_21", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 21) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_22", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 22) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_23", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 23) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_24", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 24) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_25", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 25) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_26", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 26) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_27", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 27) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_28", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 28) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_29", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 29) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_30", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 30) & 0x1), false);
                // overlay->AddOrUpdateEntity(actorId, actorName, "flags2_31", actorPtr + offsetof(ActorWiiU, flags2), BEType<uint8_t>((flags2 >> 31) & 0x1), false);
            }

            addField.operator()<uint32_t>("hashId", offsetof(ActorWiiU, hashId));
            addMemoryRange("physics", actorPtr + offsetof(ActorWiiU, actorPhysicsPtr), 0xE0);
            addMemoryRange("actorX6A0", actorPtr + offsetof(ActorWiiU, actorX6A0Ptr), 0x6C);
            addMemoryRange("chemicals", actorPtr + offsetof(ActorWiiU, chemicalsPtr), 0x64);
            addMemoryRange("reactions", actorPtr + offsetof(ActorWiiU, reactionsPtr), 0x0C);
            // addField.operator()<float>("lodDrawDistanceMultiplier", offsetof(ActorWiiU, lodDrawDistanceMultiplier));
        }

        // other systems might've added memory to the overlay, so hence this is a separate loop
        for (auto& entity : VRManager::instance().VK->m_imguiOverlay->m_entities | std::views::values) {
            if (!entity.isEntity)
                continue;

            for (auto& value : entity.values) {
                if (!value.frozen)
                    continue;

                std::visit([&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, BEType<uint32_t>>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEType<int32_t>>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEType<float>>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEVec3>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEMatrix34>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, uint8_t>) {
                        writeMemory(value.value_address, &arg);
                    }
                }, value.value);
            }
        }
    }
}

extern glm::fvec3 g_lookAtPos;
extern glm::fquat g_lookAtQuat;
extern OpenXR::EyeSide s_currentEye;

// glm::fquat rotateHorizontalCounter = glm::quat(glm::vec3(0.0f, glm::pi<float>(), 0.0f));

void vrhook_changeWeaponMtx(OpenXR::EyeSide side, BEMatrix34& toBeAdjustedMtx, BEMatrix34& defaultMtx) {
    // convert VR controller info to glm
    XrPosef handPose = VRManager::instance().XR->m_input.controllers[side].poseLocation.pose;

    // handPose.orientation.w = rotateHorizontalCounter.w;
    // handPose.orientation.x = rotateHorizontalCounter.x;
    // handPose.orientation.y = rotateHorizontalCounter.y;
    // handPose.orientation.z = rotateHorizontalCounter.z;
    // rotateHorizontalCounter = glm::rotate(rotateHorizontalCounter, glm::radians(360.0f/30.0f/1.0f), glm::fvec3(1.0f, 0.0f, 0.0f));

    glm::fvec3 controllerPos(
        handPose.position.x,
        handPose.position.y,
        handPose.position.z
    );
    glm::fquat controllerQuat(
        handPose.orientation.w,
        handPose.orientation.x,
        handPose.orientation.y,
        handPose.orientation.z
    );

    // Next, calculate the rotation
    glm::fquat rotatedControllerQuat = glm::normalize(g_lookAtQuat * controllerQuat);
    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(1.0f, 0.0f, 0.0f));
    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(0.0f, 0.0f, 1.0f));
    glm::fmat3 finalMtx = glm::toMat3(glm::inverse(rotatedControllerQuat));

    toBeAdjustedMtx.x_x = finalMtx[0][0];
    toBeAdjustedMtx.y_x = finalMtx[0][1];
    toBeAdjustedMtx.z_x = finalMtx[0][2];

    toBeAdjustedMtx.x_y = finalMtx[1][0];
    toBeAdjustedMtx.y_y = finalMtx[1][1];
    toBeAdjustedMtx.z_y = finalMtx[1][2];

    toBeAdjustedMtx.x_z = finalMtx[2][0];
    toBeAdjustedMtx.y_z = finalMtx[2][1];
    toBeAdjustedMtx.z_z = finalMtx[2][2];

    // First, calculate the position
    // Use player position as the origin since we want to overwrite the weapon position with the VR controller position
    glm::fvec3 rotatedControllerPos = g_lookAtQuat * controllerPos;
    glm::fvec3 finalPos = g_lookAtPos + rotatedControllerPos;

    toBeAdjustedMtx.pos_x = finalPos.x;
    toBeAdjustedMtx.pos_y = finalPos.y;
    toBeAdjustedMtx.pos_z = finalPos.z;
}

void CemuHooks::hook_changeWeaponMtx(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // r3 holds an actor pointer, I think?
    // r4 holds the bone name
    // r5 holds the matrix that is to be set
    // r6 holds the extra matrix that is to be set
    // r7 holds the ModelBindInfo->mtx

    hCPU->gpr[9] = 0;

    uint32_t actorLinkPtr = hCPU->gpr[3] + offsetof(ActorWiiU, baseProcPtr);
    uint32_t actorNamePtr = 0;
    readMemoryBE(actorLinkPtr, &actorNamePtr);
    if (actorNamePtr == 0)
        return;
    char* actorName = (char*)s_memoryBaseAddress + actorNamePtr;

    // read bone name
    uint32_t boneNamePtr = hCPU->gpr[4];
    if (boneNamePtr == 0)
        return;
    char* boneName = (char*)s_memoryBaseAddress + boneNamePtr;

    // real logic
    bool isHeldByPlayer = strcmp(actorName, "GameROMPlayer") == 0;
    bool isLeftHandWeapon = strcmp(boneName, "Weapon_L") == 0;
    bool isRightHandWeapon = strcmp(boneName, "Weapon_R") == 0;
    if (actorName[0] != '\0' && boneName[0] != '\0' && isHeldByPlayer && (isLeftHandWeapon || isRightHandWeapon)) {
        BEMatrix34 weaponMtx = {};
        readMemory(hCPU->gpr[5], &weaponMtx);

        BEMatrix34 playerMtx = {};
        readMemory(hCPU->gpr[6], &playerMtx);

        BEMatrix34 modelBindInfoMtx = {};
        readMemory(hCPU->gpr[7], &modelBindInfoMtx);

        vrhook_changeWeaponMtx(isLeftHandWeapon ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT, weaponMtx, playerMtx);

        // prevent weapon transparency
        BEType<float> modelOpacity = 1.0f;
        writeMemory(hCPU->gpr[8] + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
        uint8_t opacityOrSomethingEnabled = 1;
        writeMemory(hCPU->gpr[8] + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);

        writeMemory(hCPU->gpr[5], &weaponMtx);
        writeMemory(hCPU->gpr[6], &playerMtx);
        writeMemory(hCPU->gpr[7], &modelBindInfoMtx);

        hCPU->gpr[9] = 1;

        auto& m_overlay = VRManager::instance().VK->m_imguiOverlay;
        if (m_overlay) {
            // m_overlay->m_playerPos = playerMtx.getPos().getLE();

            m_overlay->AddOrUpdateEntity(1337, "PlayerHeldWeapons", isLeftHandWeapon ? "left_weapon_mtx" : "right_weapon_mtx", hCPU->gpr[5], weaponMtx);
            m_overlay->AddOrUpdateEntity(1337, "PlayerHeldWeapons", isLeftHandWeapon ? "left_player_mtx" : "right_player_mtx", hCPU->gpr[6], playerMtx);
            m_overlay->AddOrUpdateEntity(1337, "PlayerHeldWeapons", isLeftHandWeapon ? "left_ModelBindInfo_mtx" : "right_ModelBindInfo_mtx", hCPU->gpr[7], modelBindInfoMtx);
            BEVec3 zeroMtx = {-100.0f, -100.0f, -100.0f};
            m_overlay->SetPosition(1337, zeroMtx, zeroMtx);

            // freeze the value so it doesn't get overwritten
            Entity& entity = m_overlay->m_entities[1337];
            for (auto& value : entity.values) {
                if (value.value_name == (isLeftHandWeapon ? "left_weapon_mtx" : "right_weapon_mtx") && value.frozen) {
                    weaponMtx = std::get<BEMatrix34>(value.value);
                    writeMemory(hCPU->gpr[5], &weaponMtx);
                }
                else if (value.value_name == (isLeftHandWeapon ? "left_player_mtx" : "right_player_mtx") && value.frozen) {
                    playerMtx = std::get<BEMatrix34>(value.value);
                    writeMemory(hCPU->gpr[6], &playerMtx);
                }
                else if (value.value_name == (isLeftHandWeapon ? "left_ModelBindInfo_mtx" : "right_ModelBindInfo_mtx") && value.frozen) {
                    modelBindInfoMtx = std::get<BEMatrix34>(value.value);
                    writeMemory(hCPU->gpr[7], &modelBindInfoMtx);
                }
            }
        }
    }
}

void CemuHooks::hook_modifyHandModelAccessSearch(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    if (hCPU->gpr[3] == 0) {
        return;
    }

    // r3 holds the address of the string to search for
    const char* actorName = (const char*)(s_memoryBaseAddress + hCPU->gpr[3]);

    if (actorName != nullptr) {
        // Weapon_R is presumably his right hand bone name
        Log::print("Searching for model handle using {}", actorName);
    }
}

void CemuHooks::hook_CreateNewActor(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    if (VRManager::instance().XR->GetRenderer() == nullptr || VRManager::instance().XR->GetRenderer()->m_layer3D.GetStatus() == RND_Renderer::Layer3D::Status3D::UNINITIALIZED) {
        hCPU->gpr[3] = 0;
        return;
    }

    // test if controller is connected
    if (VRManager::instance().XR->m_input.controllers[0].select.currentState == XR_TRUE && VRManager::instance().XR->m_input.controllers[0].select.changedSinceLastSync == XR_TRUE) {
        Log::print("Trying to spawn new thing!");
        hCPU->gpr[3] = 1;
    }
    else if (VRManager::instance().XR->m_input.controllers[1].select.currentState == XR_TRUE && VRManager::instance().XR->m_input.controllers[1].select.changedSinceLastSync == XR_TRUE) {
        Log::print("Trying to spawn new thing!");
        hCPU->gpr[3] = 1;
    }
    else {
        hCPU->gpr[3] = 0;
    }
}
