#include "platform/mac/login/login_item.h"
#import <ServiceManagement/ServiceManagement.h>

bool setLaunchAtLogin(bool enable) {
    if (@available(macOS 13.0, *)) {
        SMAppService *service = [SMAppService mainAppService];
        NSError *error = nil;
        if (enable) {
            if (service.status == SMAppServiceStatusEnabled) return true;
            return [service registerAndReturnError:&error];
        } else {
            if (service.status != SMAppServiceStatusEnabled) return true;
            return [service unregisterAndReturnError:&error];
        }
    }
    return false;
}

bool isLaunchAtLoginEnabled() {
    if (@available(macOS 13.0, *)) {
        SMAppService *service = [SMAppService mainAppService];
        return service.status == SMAppServiceStatusEnabled;
    }
    return false;
}
