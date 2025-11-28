#pragma once

#include "IModule.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <iostream>

/*
# ModuleManager
    Управляет жизненным циклом модулей, сохраняет их атрибуты и позволяет иметь модульную структуру.

*/

class ModuleRegistry{
private:
    std::unordered_map<std::string, std::unique_ptr<IModule>> modules_;

public:
    template<typename T, typename... Args>
    T* registerModule(Args&&... args) {
        auto module = std::make_unique<T>(std::forward<Args>(args)...);
        std::string id = module->getId();

        if (modules_.find(id) != modules_.end()) {
            throw std::runtime_error("Module with id '" + id + "' already registered");
        }

        T* ptr = module.get();
        modules_[id] = std::move(module);
        return ptr;
    }

    IModule* getModule(const std::string& id) {
        auto it = modules_.find(id);
        return it != modules_.end() ? it->second.get() : nullptr;
    }

    template<typename T>
    T* getModuleAs(const std::string& id) {
        return dynamic_cast<T*>(getModule(id));
    }

    bool initializeAll() {
        bool all_ok = true;
        for (auto& [id, module] : modules_) {
            if (module->isEnabled()) {
                if (!module->initialize()) {
                    std::cerr << "Failed to initialize module: " << id << std::endl;
                    all_ok = false;
                }
            }
        }
        return all_ok;
    }

    void shutdownAll() {
        for (auto& [id, module] : modules_) {
            if (module->isEnabled()) {
                module->shutdown();
            }
        }
    }

    std::vector<std::string> getModuleIds() const {
        std::vector<std::string> ids;
        for (const auto& [id, module] : modules_) {
            ids.push_back(id);
        }
        return ids;
    }
};