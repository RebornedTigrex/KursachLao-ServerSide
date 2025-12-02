// Data Cache Module - Centralized data management
class DataCache {
    constructor(options = {}) {
        this.storageKey = options.storageKey || 'hr_data_cache_v1';
        this.cache = {
            dashboard: null,
            employees: null,
            hours: null,
            penalties: null,
            bonuses: null,
            lastUpdated: null
        };

        this.apiBaseUrl = options.apiBaseUrl || 'https://api.example.com/hr';
        this.enablePersistence = typeof options.enablePersistence === 'boolean' ? options.enablePersistence : true;

        this._loadFromStorage();
    }

    // --- Persistence helpers ---
    _loadFromStorage() {
        if (!this.enablePersistence) return;
        try {
            const raw = localStorage.getItem(this.storageKey);
            if (raw) {
                const parsed = JSON.parse(raw);
                // Keep only expected keys
                this.cache = Object.assign(this.cache, parsed);
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
        } catch (e) {}
    }

    _isCacheExpired() {
        if (!this.cache.lastUpdated) return true;
        const now = new Date();
        const last = new Date(this.cache.lastUpdated);
        const diffInMinutes = (now - last) / (1000 * 60);
        return diffInMinutes > 5;
    }

    // --- Network helper (optional sync) ---
    async _syncToServer(method, path, body) {
        if (!this.apiBaseUrl || this.apiBaseUrl.includes('example.com')) return null;
        try {
            const res = await fetch(`${this.apiBaseUrl}${path}`, {
                method,
                headers: { 'Content-Type': 'application/json' },
                body: body ? JSON.stringify(body) : undefined
            });
            if (!res.ok) {
                const text = await res.text();
                console.warn('Server responded with non-OK status', res.status, text);
                return null;
            }
            return await res.json();
        } catch (err) {
            console.warn('Failed to sync with server:', err);
            return null;
        }
    }

    setOptions(opts = {}) {
        if (typeof opts.apiBaseUrl === 'string') this.apiBaseUrl = opts.apiBaseUrl;
        if (typeof opts.enablePersistence === 'boolean') this.enablePersistence = opts.enablePersistence;
        if (opts.storageKey) this.storageKey = opts.storageKey;
        this._saveToStorage();
    }

    // --- Public API ---
    async getDashboardData() {
        if (!this.cache.dashboard || this._isCacheExpired()) {
            // try fetching from server if configured
            if (this.apiBaseUrl && !this.apiBaseUrl.includes('example.com')) {
                const server = await this._syncToServer('GET', '/dashboard');
                if (server) {
                    this.cache.dashboard = server;
                    this._markUpdated();
                    return this.cache.dashboard;
                }
            }

            // fallback/mock
            this.cache.dashboard = {
                penalties: Math.floor(Math.random() * 10),
                bonuses: Math.floor(Math.random() * 5),
                undertime: Math.floor(Math.random() * 20)
            };
            this._markUpdated();
        }
        return this.cache.dashboard;
    }

    async getHoursForEmployee(employeeId) {
        if (!this.cache.hours) this.cache.hours = [];

        let hours = this.cache.hours.find(h => h.employeeId === employeeId);
        if (hours) return hours;

        // Try server
        if (this.apiBaseUrl && !this.apiBaseUrl.includes('example.com')) {
            const server = await this._syncToServer('GET', `/hours/${employeeId}`);
            if (server) {
                this.cache.hours.push(server);
                this._markUpdated();
                return server;
            }
        }

        // default
        hours = { employeeId, regularHours: 0, overtime: 0, undertime: 0 };
        this.cache.hours.push(hours);
        this._markUpdated();
        return hours;
    }

    async getEmployees() {
        if (!this.cache.employees || this._isCacheExpired()) {
            // Try server first
            if (this.apiBaseUrl && !this.apiBaseUrl.includes('example.com')) {
                const server = await this._syncToServer('GET', '/employees');
                if (server && Array.isArray(server)) {
                    this.cache.employees = server;
                    this._markUpdated();
                    return this.cache.employees;
                }
            }

            // Fallback/mock data (kept for offline/dev)
            this.cache.employees = [
                { id: 1, fullname: 'John Doe', status: 'hired', salary: 50000, penalties: 2, bonuses: 1 },
                { id: 2, fullname: 'Jane Smith', status: 'hired', salary: 65000, penalties: 0, bonuses: 3 },
                { id: 3, fullname: 'Mike Johnson', status: 'fired', salary: 45000, penalties: 5, bonuses: 0 },
                { id: 4, fullname: 'Sarah Williams', status: 'interview', salary: 55000, penalties: 0, bonuses: 0 }
            ];
            this._markUpdated();
        }
        return this.cache.employees;
    }

    async addEmployee(employeeData) {
        const newEmployee = Object.assign({}, employeeData, {
            id: Math.floor(Math.random() * 100000) + 1000,
            penalties: employeeData.penalties || 0,
            bonuses: employeeData.bonuses || 0
        });

        if (!this.cache.employees) this.cache.employees = [];
        this.cache.employees.push(newEmployee);

        // initialize hours entry
        if (!this.cache.hours) this.cache.hours = [];
        this.cache.hours.push({ employeeId: newEmployee.id, regularHours: 0, overtime: 0, undertime: 0 });

        this._markUpdated();

        // Try to persist to server
        const serverResp = await this._syncToServer('POST', '/employees', newEmployee);
        if (serverResp && serverResp.id) {
            // update id if server assigned one
            const idx = this.cache.employees.findIndex(e => e.id === newEmployee.id);
            if (idx !== -1) {
                this.cache.employees[idx] = Object.assign(this.cache.employees[idx], serverResp);
                this._markUpdated();
            }
        }

        return newEmployee;
    }

    async updateEmployee(employeeId, changes) {
        if (!this.cache.employees) this.cache.employees = [];
        const emp = this.cache.employees.find(e => e.id === employeeId);
        if (!emp) throw new Error('Employee not found');
        Object.assign(emp, changes);
        this._markUpdated();
        await this._syncToServer('PUT', `/employees/${employeeId}`, emp);
        return emp;
    }

    async addHours(employeeId, hoursData) {
        if (!this.cache.hours) this.cache.hours = [];
        let existing = this.cache.hours.find(h => h.employeeId === employeeId);
        if (existing) {
            // merge numeric fields
            existing.regularHours = Number(hoursData.regularHours ?? existing.regularHours);
            existing.overtime = Number(hoursData.overtime ?? existing.overtime);
            existing.undertime = Number(hoursData.undertime ?? existing.undertime);
        } else {
            existing = Object.assign({ employeeId }, hoursData);
            this.cache.hours.push(existing);
        }
        this._markUpdated();
        await this._syncToServer('POST', `/hours/${employeeId}`, existing);
    }

    async addPenalty(employeeId, penaltyData) {
        if (!this.cache.penalties) this.cache.penalties = [];
        const penalty = { id: Date.now(), employeeId, ...penaltyData, date: new Date().toISOString() };
        this.cache.penalties.push(penalty);

        // update counter
        const employee = this.cache.employees?.find(e => e.id === employeeId);
        if (employee) employee.penalties = (employee.penalties || 0) + 1;

        this._markUpdated();
        await this._syncToServer('POST', `/employees/${employeeId}/penalties`, penalty);
    }

    async addBonus(employeeId, bonusData) {
        if (!this.cache.bonuses) this.cache.bonuses = [];
        const bonus = { id: Date.now(), employeeId, ...bonusData, date: new Date().toISOString() };
        this.cache.bonuses.push(bonus);

        const employee = this.cache.employees?.find(e => e.id === employeeId);
        if (employee) employee.bonuses = (employee.bonuses || 0) + 1;

        this._markUpdated();
        await this._syncToServer('POST', `/employees/${employeeId}/bonuses`, bonus);
    }

    clearCache() {
        this.cache = { dashboard: null, employees: null, hours: null, penalties: null, bonuses: null, lastUpdated: null };
        try { localStorage.removeItem(this.storageKey); } catch (e) {}
    }
}

// Initialize and expose the data cache (preserve existing instance if present)
if (!window.dataCache) {
    window.dataCache = new DataCache();
}