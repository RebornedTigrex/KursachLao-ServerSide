// modules/notifications.js - Module for handling dynamic notifications with offline suppression

window.notifications = (function () {
    const container = document.getElementById('notification-container');

    function createNotificationElement(message, type) {
        const div = document.createElement('div');
        div.className = `px-4 py-2 rounded-lg shadow-lg ${type === 'success' ? 'bg-green-500 text-white' : 'bg-red-500 text-white'
            }`;
        div.innerHTML = message;
        return div;
    }

    function showNotification(message, type = 'success', duration = 3000) {
        container.innerHTML = ''; // Clear previous notifications
        const notification = createNotificationElement(message, type);
        container.appendChild(notification);
        container.classList.remove('hidden');

        setTimeout(() => {
            container.classList.add('hidden');
            container.innerHTML = '';
        }, duration);
    }

    function showSuccess(message) {
        showNotification(message, 'success');
    }

    function showError(error) {
        // Проверяем, если оффлайн-режим — не показываем ошибку
        if (window.dataCache?.isOffline()) {
            console.warn('Offline mode: suppressing error notification', error);
            return; // Silent — нет попапа
        }

        let errorMessage = 'Ошибка операции';
        if (error instanceof Error) {
            errorMessage += `: ${error.message}`;
        } else if (error.status) {
            errorMessage += ` (Код: ${error.status})`;
        }

        // Dynamic JSON parsing if response body exists
        if (error.body) {
            try {
                const errorData = JSON.parse(error.body);
                Object.entries(errorData).forEach(([key, value]) => {
                    errorMessage += `<br>${key.charAt(0).toUpperCase() + key.slice(1)}: ${value}`;
                });
            } catch (parseError) {
                errorMessage += `<br>Детали: ${error.body}`;
            }
        }

        showNotification(errorMessage, 'error', 5000); // Longer duration for errors
    }

    return {
        showSuccess,
        showError
    };
})();