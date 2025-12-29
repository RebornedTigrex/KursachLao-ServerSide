class DataCache {
    constructor(options = {}) {
        this.storageKey = options.storageKey || 'hr_data_cache_v1';
        this.cache = {
            dashboard: null,
            employees: [],
            hours: [], // Array of {employeeId, regularHours, overtime, undertime}
            penalties: [], // Array of {id, employeeId, reason, amount, date}
            bonuses: [], // Array of {id, employeeId, note, amount, date}
            lastUpdated: null
        };

        this.apiBaseUrl = options.apiBaseUrl || '/api';
        this.enablePersistence = typeof options.enablePersistence === 'boolean' ? options.enablePersistence : true;
        this.isOfflineMode = false; // Новый флаг для оффлайн-режима

        this._loadFromStorage();
    }

    // --- Persistence helpers ---
    _loadFromStorage() {
        if (!this.enablePersistence) return;
        try {
            const raw = localStorage.getItem(this.storageKey);
            if (raw) {
                const parsed = JSON.parse(raw);
                this.cache = { ...this.cache, ...parsed };
            }
        } catch (err) {
            console.warn('Failed to load cache from localStorage:', err);
        }
    }

    _saveToStorage() {
        if (!this.enablePersistence) return;
        try {
            localStorage.setItem(this.storageKey, JSON.stringify(this.cache));
        } catch (err) {
            console.warn('Failed to save cache to localStorage:', err);
        }
    }

    _markUpdated() {
        this.cache.lastUpdated = new Date().toISOString();
        this._saveToStorage();
        try {
            window.dispatchEvent(new CustomEvent('dataCache:updated', { detail: { lastUpdated: this.cache.lastUpdated } }));
        } catch (e) { }
    }

    _isCacheExpired() {
        if (!this.cache.lastUpdated) return true;
        const now = new Date();
        const last = new Date(this.cache.lastUpdated);
        const diffInMinutes = (now - last) / (1000 * 60);
        return diffInMinutes > 5; // 5-minute cache TTL
    }

    // --- Network helper with structured error object ---
    async _syncToServer(method, path, body, options = {}) {
        try {
            const url = `${this.apiBaseUrl}${path}`;
            const headers = { 'Content-Type': 'application/json' };
            // Placeholder for future auth
            // headers['Authorization'] = 'Bearer ' + localStorage.getItem('authToken');

            const res = await fetch(url, {
                method,
                headers,
                body: body ? JSON.stringify(body) : undefined,
                ...options
            });

            if (!res.ok) {
                const text = await res.text();
                const error = {
                    status: res.status,
                    statusText: res.statusText,
                    body: text
                };
                throw error;
            }

            const data = await res.json();

            if (data.lastUpdated && new Date(data.lastUpdated) > new Date(this.cache.lastUpdated || 0)) {
                this.cache.lastUpdated = data.lastUpdated;
            }
            this.isOfflineMode = false; // Сброс оффлайн-режима при успехе
            return data;
        } catch (err) {
            console.warn('Network error, switching to offline mode:', err);
            this.isOfflineMode = true; // Включаем оффлайн-режим при любой ошибке (503, сеть и т.д.)
            throw err; // Пробрасываем для обработки в CRUD
        }
    }

    isOffline() {
        return this.isOfflineMode;
    }

    setOptions(opts = {}) {
        if (typeof opts.apiBaseUrl === 'string') this.apiBaseUrl = opts.apiBaseUrl;
        if (typeof opts.enablePersistence === 'boolean') this.enablePersistence = opts.enablePersistence;
        if (opts.storageKey) this.storageKey = opts.storageKey;
        this._saveToStorage();
    }

    // --- Public API ---
    async fetchAllData(forceRefresh = false) {
        if (!forceRefresh && !this._isCacheExpired()) {
            return this.cache;
        }

        try {
            const serverData = await this._syncToServer('GET', '/all-data');
            if (serverData) {
                this.cache = { ...this.cache, ...serverData };
                this._markUpdated();
                return this.cache;
            }
        } catch (err) {
            console.warn('Using local cache due to network error:', err);
            // Не throw здесь — возвращаем локальные данные
        }

        // Mock/fallback only if cache is empty
        if (!this.cache.employees.length) {
            this.cache.employees = [
                { id: 1, fullname: 'John Doe', status: 'hired', salary: 50000, penalties: 2, bonuses: 1, totalPenalties: 400, totalBonuses: 500 },
                { id: 2, fullname: 'Jane Smith', status: 'hired', salary: 65000, penalties: 0, bonuses: 3, totalPenalties: 0, totalBonuses: 1500 },
                { id: 3, fullname: 'Mike Johnson', status: 'fired', salary: 45000, penalties: 5, bonuses: 0, totalPenalties: 1000, totalBonuses: 0 },
                { id: 4, fullname: 'Sarah Williams', status: 'interview', salary: 55000, penalties: 0, bonuses: 0, totalPenalties: 0, totalBonuses: 0 }
            ];
            this.cache.hours = [
                { employeeId: 1, regularHours: 160, overtime: 10, undertime: 2 },
                { employeeId: 2, regularHours: 160, overtime: 5, undertime: 0 },
                { employeeId: 3, regularHours: 120, overtime: 0, undertime: 40 },
                { employeeId: 4, regularHours: 0, overtime: 0, undertime: 0 }
            ];
            this.cache.penalties = [];
            this.cache.bonuses = [];
            this._computeDashboard();
        }
        this._markUpdated();
        return this.cache;
    }

    _computeDashboard() {
        const hiredEmployees = this.cache.employees.filter(e => e.status === 'hired');
        this.cache.dashboard = {
            penalties: hiredEmployees.reduce((sum, e) => sum + (e.penalties || 0), 0),
            bonuses: hiredEmployees.reduce((sum, e) => sum + (e.bonuses || 0), 0),
            undertime: this.cache.hours
                .filter(h => hiredEmployees.some(e => e.id === h.employeeId))
                .reduce((sum, h) => sum + (h.undertime || 0), 0)
        };
    }

    async getDashboardData() {
        await this.fetchAllData();
        return this.cache.dashboard;
    }

    async getEmployees() {
        await this.fetchAllData();
        return this.cache.employees;
    }

    async getHoursForEmployee(employeeId) {
        await this.fetchAllData();
        return this.cache.hours.find(h => h.employeeId === employeeId) || { employeeId, regularHours: 0, overtime: 0, undertime: 0 };
    }

    // ---------- CRUD operations with proper error propagation ----------
    async addEmployee(employeeData) {
        const payloadForServer = {
            fullname: employeeData.fullname.trim(),
            status: employeeData.status || 'interview',
            salary: Number(employeeData.salary)
        };

        const tempId = Date.now();
        const newEmployee = {
            id: tempId,
            ...payloadForServer,
            penalties: 0,
            bonuses: 0,
            totalPenalties: 0,
            totalBonuses: 0
        };

        // Optimistic UI update
        this.cache.employees.push(newEmployee);
        this.cache.hours.push({ employeeId: tempId, regularHours: 0, overtime: 0, undertime: 0 });
        this._computeDashboard();
        this._markUpdated();

        try {
            const serverResp = await this._syncToServer('POST', '/employees', payloadForServer);

            if (serverResp && serverResp.id) {
                const empIdx = this.cache.employees.findIndex(e => e.id === tempId);
                if (empIdx !== -1) {
                    this.cache.employees[empIdx] = { ...this.cache.employees[empIdx], ...serverResp };
                }
                const hoursIdx = this.cache.hours.findIndex(h => h.employeeId === tempId);
                if (hoursIdx !== -1) {
                    this.cache.hours[hoursIdx].employeeId = serverResp.id;
                }
                await this.fetchAllData(true);
            }
        } catch (error) {
            if (!this.isOfflineMode) throw error; // Не бросаем ошибку в оффлайн-режиме
        }

        return newEmployee;
    }

    async updateEmployee(employeeId, changes) {
        const emp = this.cache.employees.find(e => e.id === employeeId);
        if (!emp) throw new Error('Employee not found');

        Object.assign(emp, changes);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('PUT', `/employees/${employeeId}`, emp);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }

        return emp;
    }

    async addHours(employeeId, hoursData) {
        let existing = this.cache.hours.find(h => h.employeeId === employeeId);
        if (existing) {
            existing.regularHours = Number(hoursData.regularHours ?? existing.regularHours);
            existing.overtime = Number(hoursData.overtime ?? existing.overtime);
            existing.undertime = Number(hoursData.undertime ?? existing.undertime);
        } else {
            existing = { employeeId, ...hoursData };
            this.cache.hours.push(existing);
        }
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('POST', `/hours/${employeeId}`, existing);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    async addPenalty(employeeId, penaltyData) {
        const penalty = { id: Date.now(), employeeId, ...penaltyData, date: new Date().toISOString() };
        this.cache.penalties.push(penalty);

        const employee = this.cache.employees.find(e => e.id === employeeId);
        if (employee) {
            employee.penalties = (employee.penalties || 0) + 1;
            employee.totalPenalties = (employee.totalPenalties || 0) + (penalty.amount || 0);
        }
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('POST', `/employees/${employeeId}/penalties`, penalty);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    async addBonus(employeeId, bonusData) {
        const bonus = { id: Date.now(), employeeId, ...bonusData, date: new Date().toISOString() };
        this.cache.bonuses.push(bonus);

        const employee = this.cache.employees.find(e => e.id === employeeId);
        if (employee) {
            employee.bonuses = (employee.bonuses || 0) + 1;
            employee.totalBonuses = (employee.totalBonuses || 0) + (bonus.amount || 0);
        }
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('POST', `/employees/${employeeId}/bonuses`, bonus);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    clearCache() {
        this.cache = { dashboard: null, employees: [], hours: [], penalties: [], bonuses: [], lastUpdated: null };
        try { localStorage.removeItem(this.storageKey); } catch (e) { }
        this.isOfflineMode = false; // Сброс при очистке
    }
}

// Initialize and expose globally
if (!window.dataCache) {
    window.dataCache = new DataCache();
}