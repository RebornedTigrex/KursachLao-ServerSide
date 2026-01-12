// modules/serverSync.js - Модуль для синхронизации с сервером, обработки оффлайн-режима и очереди действий

class ServerSync {
    constructor(options = {}) {
        this.apiBaseUrl = options.apiBaseUrl || '/api';
        this.storageKey = 'serverSync_pendingQueue_v1'; // Ключ для хранения очереди в localStorage
        this.isOffline = !navigator.onLine; // Начальное состояние на основе navigator
        this.pendingQueue = []; // Очередь отложенных действий: [{method, path, body}]

        this.loadQueue(); // Загружаем очередь из хранилища
        this.init(); // Инициализируем слушатели событий
    }

    // Инициализация слушателей онлайн/оффлайн событий
    init() {
        window.addEventListener('online', this.handleOnline.bind(this));
        window.addEventListener('offline', this.handleOffline.bind(this));

        // Если сейчас онлайн, обрабатываем очередь
        if (!this.isOffline) {
            this.processQueue();
        }
    }

    // Обработчик восстановления соединения
    handleOnline() {
        this.isOffline = false;
        this.processQueue();
    }

    // Обработчик потери соединения
    handleOffline() {
        this.isOffline = true;
    }

    // Загрузка очереди из localStorage
    loadQueue() {
        try {
            const stored = localStorage.getItem(this.storageKey);
            if (stored) {
                this.pendingQueue = JSON.parse(stored);
            }
        } catch (error) {
            console.warn('Ошибка загрузки очереди из localStorage:', error);
        }
    }

    // Сохранение очереди в localStorage
    saveQueue() {
        try {
            localStorage.setItem(this.storageKey, JSON.stringify(this.pendingQueue));
        } catch (error) {
            console.warn('Ошибка сохранения очереди в localStorage:', error);
        }
    }

    // Основной метод синхронизации: выполняет запрос или добавляет в очередь
    async sync(method, path, body = null) {
        if (this.isOffline) {
            // Если оффлайн, добавляем в очередь и выбрасываем ошибку для локальной обработки
            this.pendingQueue.push({ method, path, body });
            this.saveQueue();
            throw new Error('Оффлайн-режим: действие добавлено в очередь');
        }

        const fullPath = this.apiBaseUrl + path;
        try {
            const response = await fetch(fullPath, {
                method,
                headers: { 'Content-Type': 'application/json' },
                body: body ? JSON.stringify(body) : undefined
            });

            if (!response.ok) {
                const errorText = await response.text();
                const errorObj = { status: response.status, statusText: response.statusText, body: errorText };
                throw errorObj;
            }

            return await response.json();
        } catch (error) {
            // Если ошибка (например, сеть), переходим в оффлайн и добавляем в очередь
            this.isOffline = true;
            this.pendingQueue.push({ method, path, body });
            this.saveQueue();
            throw error;
        }
    }

    // Обработка очереди: выполняет отложенные действия по порядку
    async processQueue() {
        let i = 0;
        while (i < this.pendingQueue.length) {
            const action = this.pendingQueue[i];
            const fullPath = this.apiBaseUrl + action.path;

            try {
                const response = await fetch(fullPath, {
                    method: action.method,
                    headers: { 'Content-Type': 'application/json' },
                    body: action.body ? JSON.stringify(action.body) : undefined
                });

                if (response.ok) {
                    // Успех: удаляем из очереди
                    this.pendingQueue.splice(i, 1);
                    this.saveQueue();
                } else {
                    // Ошибка: оставляем в очереди и переходим к следующему
                    i++;
                }
            } catch (error) {
                // Сетевая ошибка: оставляем и прерываем обработку
                i++;
                this.isOffline = true;
                break;
            }
        }

        this.saveQueue();

        // После обработки очереди обновляем кэш в dataCache (если доступен)
        if (window.dataCache) {
            try {
                await window.dataCache.fetchAllData(true);
            } catch (error) {
                console.warn('Ошибка обновления dataCache после синхронизации:', error);
            }
        }

        // Если очередь очищена, показываем уведомление (если notifications доступен)
        if (this.pendingQueue.length === 0 && window.notifications) {
            window.notifications.showSuccess('Данные успешно синхронизированы с сервером');
        }
    }
}

// Глобальный экземпляр модуля
if (!window.serverSync) {
    window.serverSync = new ServerSync();
}