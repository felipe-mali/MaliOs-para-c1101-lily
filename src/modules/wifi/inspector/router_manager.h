#pragma once
#include "device_model.h"
namespace WifiInspector {
enum class ManagementResult { Unsupported, Authenticated, AuthFailed, Failed, Requested, Disconnected };
// Adapters must target only the selected association. No ban lists or radio fallback.
// Requested is distinct from a verified disconnection.
class RouterManager {
public:
    virtual ~RouterManager()=default;
    virtual bool isManagementSupported() const=0;
    virtual ManagementResult authenticate()=0;
    virtual size_t getConnectedClients(Mac *clients,size_t capacity)=0;
    virtual ManagementResult disconnectClient(const Mac &mac)=0;
    virtual void logout()=0;
};
class GenericRouter final : public RouterManager {
public:
    bool isManagementSupported() const override { return false; }
    ManagementResult authenticate() override { return ManagementResult::Unsupported; }
    size_t getConnectedClients(Mac *,size_t) override { return 0; }
    ManagementResult disconnectClient(const Mac &) override { return ManagementResult::Unsupported; }
    void logout() override {}
};
}
