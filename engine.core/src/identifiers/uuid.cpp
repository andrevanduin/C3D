
#include "uuid.h"

#include <random>

namespace C3D
{
    static std::random_device sRandomDevice;
    static std::mt19937_64 sRandomEngine(sRandomDevice());
    static std::uniform_int_distribution<u64> sUniformDistribution;

    UUID::UUID() {}

    UUID::UUID(u64 uuid) : m_uuid(uuid) {}

    UUID::UUID(const UUID& other) : m_uuid(other.m_uuid) {}

    void UUID::Generate() { m_uuid = sUniformDistribution(sRandomEngine); }

    void UUID::Invalidate() { m_uuid = INVALID_ID_U64; }

    UUID UUID::Create()
    {
        auto uuid = UUID();
        uuid.Generate();
        return uuid;
    }

    UUID UUID::Invalid() { return UUID(); }

}  // namespace C3D
