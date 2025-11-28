#pragma once
#include "IModule.h"
#include <atomic>

class BaseModule : public IModule {
protected:
    std::string id_;
    std::string name_;
    std::string version_;
    std::atomic<bool> enabled_;
    std::atomic<bool> initialized_;

public:
    BaseModule(const std::string& id, const std::string& name, const std::string& version)
        : id_(id), name_(name), version_(version), enabled_(true), initialized_(false) {
    }

    virtual ~BaseModule() = default;

    // IModule implementation
    std::string getId() const override { return id_; }
    std::string getName() const override { return name_; }
    std::string getVersion() const override { return version_; }
    bool isEnabled() const override { return enabled_.load(); }
    void setEnabled(bool enabled) override { enabled_.store(enabled); }

    bool initialize() override {
        if (!enabled_ || initialized_) return false;

        bool result = onInitialize();
        if (result) {
            initialized_.store(true);
        }
        return result;
    }

    void shutdown() override {
        if (initialized_) {
            onShutdown();
            initialized_.store(false);
        }
    }

protected:
    // Для реализации в наследниках
    virtual bool onInitialize() = 0;
    virtual void onShutdown() = 0;

    // Вспомогательные методы
    bool isInitialized() const { return initialized_.load(); }
};