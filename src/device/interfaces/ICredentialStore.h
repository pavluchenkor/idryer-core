#pragma once

#include "../../core/types.h"

namespace idryer {

class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;

    virtual bool begin() = 0;
    virtual bool load(DeviceIdentity& identity) = 0;
    virtual bool save(const DeviceIdentity& identity) = 0;
    virtual void clear() = 0;
};

} // namespace idryer
