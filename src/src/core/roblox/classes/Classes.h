#pragma once
#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <array>
#include "core/roblox/math/Math.h"

#ifdef GetClassName
#undef GetClassName
#endif

namespace Cheat {

    class Instance
    {
    public:
        Instance() = default;
        explicit Instance(std::uint64_t addr) : address(addr) {}
        virtual ~Instance() = default;

        std::uint64_t address{ 0 };

        std::string GetName() const;
        std::string GetClassName() const;
        bool HasChildren() const;
        std::vector<Instance> GetChildren() const;
        std::shared_ptr<Instance> FindFirstChild(std::string child) const;
        std::shared_ptr<Instance> GetParent() const;
        // external parent: Children + Parent write
        bool SetParent(std::uint64_t parent_addr) const;
    };

    class DataModel : public Instance
    {
    public:
        DataModel() = default;
        explicit DataModel(std::uint64_t addr) : Instance(addr) {}

        std::uint64_t GetGameId() const;
        std::uint64_t GetPlaceId() const;
        std::uint64_t GetCreatorId() const;
        std::int32_t GetPlaceVersion() const;
        std::string GetJobId() const;
        bool IsGameLoaded() const;
    };

    class Workspace : public Instance
    {
    public:
        Workspace() = default;
        explicit Workspace(std::uint64_t addr) : Instance(addr) {}

        std::shared_ptr<Instance> GetCurrentCamera() const;
        double GetDistributedGameTime() const;
        float GetGravity() const;
    };

    class Players : public Instance
    {
    public:
        Players() = default;
        explicit Players(std::uint64_t addr) : Instance(addr) {}

        std::vector<std::shared_ptr<Instance>> GetPlayers() const;
    };

    class Player : public Instance
    {
    public:
        Player() = default;
        explicit Player(std::uint64_t addr) : Instance(addr) {}

        std::string GetDisplayName() const;
        std::int64_t GetUserId() const;
        std::int32_t GetAccountAge() const;
        float GetMaxZoomDistance() const;
        float GetMinZoomDistance() const;
        bool IsLocalPlayer() const;
        std::uint64_t GetTeam() const;

        std::uint64_t GetCharacterAddress() const;
        std::shared_ptr<Instance> GetCharacter() const;
    };

    class Model : public Instance
    {
    public:
        Model() = default;
        explicit Model(std::uint64_t addr) : Instance(addr) {}

        std::shared_ptr<Instance> GetPrimaryPart() const;
        float GetScale() const;
    };

    class BasePart : public Instance
    {
    public:
        BasePart() = default;
        explicit BasePart(std::uint64_t addr) : Instance(addr) {}

        Vector3 GetPosition() const;
        Vector3 GetSize() const;
        Matrix4x4 GetRotation() const;
        Vector3 GetAssemblyLinearVelocity() const;
        Vector3 GetAssemblyAngularVelocity() const;
        void SetPosition(const Vector3& pos) const;
        void SetSize(const Vector3& size) const;
        void SetAssemblyLinearVelocity(const Vector3& vel) const;
        void SetCanCollide(bool value) const;
        void SetAnchored(bool value) const;
        void SetTransparency(float value) const;
        void SetColor(const Color3& value) const;
        void Invalidate() const;
        Color3 GetColor() const;
        float GetTransparency() const;
        float GetReflectance() const;
        bool IsAnchored() const;
        bool CanCollide() const;
        bool CastShadow() const;
    };

    class Part : public BasePart
    {
    public:
        Part() = default;
        explicit Part(std::uint64_t addr) : BasePart(addr) {}
    };

    class MeshPart : public BasePart
    {
    public:
        MeshPart() = default;
        explicit MeshPart(std::uint64_t addr) : BasePart(addr) {}

        std::string GetMeshId() const;
        std::string GetTexture() const;
    };

    class Humanoid : public Instance
    {
    public:
        Humanoid() = default;
        explicit Humanoid(std::uint64_t addr) : Instance(addr) {}

        float GetHealth() const;
        float GetMaxHealth() const;
        float GetWalkSpeed() const;
        float GetJumpPower() const;
        float GetJumpHeight() const;
        float GetHipHeight() const;
        Vector3 GetMoveDirection() const;
        std::string GetDisplayName() const;
        bool IsWalking() const;
        bool GetAutoRotate() const;
        bool GetAutoJump() const;

        std::int32_t GetStateId() const;

        void SetHealth(float value) const;
        void SetMaxHealth(float value) const;
        void SetWalkSpeed(float value) const;
        void SetJumpPower(float value) const;
        void SetJumpHeight(float value) const;

        std::uint64_t GetRootPartAddress() const;
        std::shared_ptr<Instance> GetRootPart() const;
    };

    class Character : public Model
    {
    public:
        Character() = default;
        explicit Character(std::uint64_t addr) : Model(addr) {}

        std::shared_ptr<Instance> GetHumanoid() const;
        std::shared_ptr<Instance> GetRootPart() const;
        std::shared_ptr<Instance> GetHead() const;
    };

    class Camera : public Instance
    {
    public:
        Camera() = default;
        explicit Camera(std::uint64_t addr) : Instance(addr) {}

        Vector3 GetPosition() const;
        Matrix4x4 GetRotation() const;
        float GetFieldOfView() const;
        Vector2 GetViewportSize() const;
        bool WorldToScreen(const Vector3& worldPos, Vector2& screenPos) const;

        void SetFieldOfView(float fov) const;
        void SetRotation(const Matrix4x4& rot) const;
    };

    class Lighting : public Instance
    {
    public:
        Lighting() = default;
        explicit Lighting(std::uint64_t addr) : Instance(addr) {}

        Color3 GetAmbient() const;
        Color3 GetOutdoorAmbient() const;
        float GetBrightness() const;
        float GetClockTime() const;
        float GetFogStart() const;
        float GetFogEnd() const;
        Color3 GetFogColor() const;
        bool GetGlobalShadows() const;

        void SetBrightness(float value) const;
        void SetClockTime(float value) const;
        void SetFogStart(float value) const;
        void SetFogEnd(float value) const;
        void SetGlobalShadows(bool value) const;
        void SetAmbient(const Color3& value) const;
        void SetOutdoorAmbient(const Color3& value) const;
        void SetFogColor(const Color3& value) const;
    };

}
