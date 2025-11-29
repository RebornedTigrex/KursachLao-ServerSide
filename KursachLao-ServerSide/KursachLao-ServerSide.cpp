#include "ModuleRegistry.h"
#include "RequestHandler.h"
#include <iostream>
#include <csignal>
#include <thread>

std::atomic<bool> running{ true };

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running.store(false);
}

int main() {
    // Устанавливаем обработчики сигналов
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        ModuleRegistry registry;

        // Регистрируем HTTP-модуль на порту 8080
        auto* httpModule = registry.registerModule<RequestHandler>();

        // Инициализируем все модули
        if (!registry.initializeAll()) {
            std::cerr << "Failed to initialize all modules" << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "All modules initialized successfully" << std::endl;
        std::cout << "Server running on http://localhost:8080" << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;

        // Главный цикл
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Корректное завершение
        std::cout << "Shutting down modules..." << std::endl;
        registry.shutdownAll();

        std::cout << "Application terminated successfully" << std::endl;
        return EXIT_SUCCESS;

    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
