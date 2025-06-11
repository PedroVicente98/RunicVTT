//#pragma once
//
//#include <string>
//#include <optional>
//#include <cstring>
//#include <iostream>
//
//#include "miniupnpc/miniupnpc.h"
//#include "miniupnpc/upnpcommands.h"
//#include "miniupnpc/upnperrors.h"
//
//class UPnPManager {
//public:
//    UPnPManager()
//        : devlist_(nullptr), initialized_(false) {
//        memset(&urls_, 0, sizeof(urls_));
//        memset(&data_, 0, sizeof(data_));
//        memset(lanaddr_, 0, sizeof(lanaddr_));
//    }
//
//    ~UPnPManager() {
//        if (initialized_) {
//            FreeUPNPUrls(&urls_);
//        }
//        if (devlist_) {
//            freeUPNPDevlist(devlist_);
//        }
//    }
//
//    bool initialize() {
//        int error = 0;
//        devlist_ = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);
//        if (!devlist_) {
//            std::cerr << "[UPnPManager] No UPnP devices found (error: " << error << ")\n";
//            return false;
//        }
//
//        int r = UPNP_GetValidIGD(devlist_, &urls_, &data_, lanaddr_, sizeof(lanaddr_));
//        if (r == 1) {
//            std::cout << "[UPnPManager] Found valid IGD: " << urls_.controlURL << "\n";
//            initialized_ = true;
//            return true;
//        }
//        else {
//            std::cerr << "[UPnPManager] No valid IGD found (code " << r << ")\n";
//            return false;
//        }
//    }
//
//    bool addPortMapping(int port, const std::string& description) {
//        if (!initialized_) return false;
//
//        std::string portStr = std::to_string(port);
//        int r = UPNP_AddPortMapping(
//            urls_.controlURL,
//            data_.first.servicetype,
//            portStr.c_str(),
//            portStr.c_str(),
//            lanaddr_,
//            description.c_str(),
//            "TCP",
//            nullptr,
//            "0"
//        );
//
//        if (r != UPNPCOMMAND_SUCCESS) {
//            std::cerr << "[UPnPManager] AddPortMapping failed: " << strupnperror(r) << "\n";
//            return false;
//        }
//
//        std::cout << "[UPnPManager] Port " << port << " mapped successfully.\n";
//        return true;
//    }
//
//    bool removePortMapping(int port) {
//        if (!initialized_) return false;
//
//        std::string portStr = std::to_string(port);
//        int r = UPNP_DeletePortMapping(
//            urls_.controlURL,
//            data_.first.servicetype,
//            portStr.c_str(),
//            "TCP",
//            nullptr
//        );
//
//        if (r != UPNPCOMMAND_SUCCESS) {
//            std::cerr << "[UPnPManager] DeletePortMapping failed: " << strupnperror(r) << "\n";
//            return false;
//        }
//
//        std::cout << "[UPnPManager] Port " << port << " unmapped successfully.\n";
//        return true;
//    }
//
//    std::optional<std::string> getExternalIPAddress() {
//        if (!initialized_) return std::nullopt;
//
//        char externalIP[40];
//        int r = UPNP_GetExternalIPAddress(
//            urls_.controlURL,
//            data_.first.servicetype,
//            externalIP
//        );
//
//        if (r != UPNPCOMMAND_SUCCESS || strlen(externalIP) == 0) {
//            std::cerr << "[UPnPManager] Failed to get external IP address.\n";
//            return std::nullopt;
//        }
//
//        return std::string(externalIP);
//    }
//
//private:
//    struct UPNPDev* devlist_;
//    struct UPNPUrls urls_;
//    struct IGDdatas data_;
//    char lanaddr_[64];
//    bool initialized_;
//};
//

