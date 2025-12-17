// dataCache.js - Improved Data Cache Module with better server sync and batch fetching

class DataCache {
    constructor(options = {}) {
        this.storageKey = options.storageKey || 'hr_data_cache_v1';
        this.cache = {
            dashboard: null,
            employees: [],
            hours: [], // Array of {employeeId, regularHours, overtime, undertime}
            penalties: [], // Array of {id, employeeId, reason, amount, date}
            bonuses: [], // Array of {id, employeeId, note, amount, date} - Standardized to 'note' instead of 'reason'
            lastUpdated: null
        };

        this.apiBaseUrl = options.apiBaseUrl || '/api'; // Changed to relative path for real backend
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
                this.cache = { ...this.cache, ...parsed }; // Merge with defaults
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
        return diffInMinutes > 5; // 5-minute cache TTL
    }

    // --- Network helper with improved error handling and auth (placeholder) ---
    async _syncToServer(method, path, body, options = {}) {
        try {
            const url = `${this.apiBaseUrl}${path}`;
            const headers = { 'Content-Type': 'application/json' };
            // Placeholder for auth: headers['Authorization'] = 'Bearer ' + localStorage.getItem('authToken');

            const res = await fetch(url, {
                method,
                headers,
                body: body ? JSON.stringify(body) : undefined,
                ...options
            });

            if (!res.ok) {
                const text = await res.text();
                throw new Error(`Server error: ${res.status} - ${text}`);
            }

            const data = await res.json();
            // Check for server lastUpdated to detect changes
            if (data.lastUpdated && new Date(data.lastUpdated) > new Date(this.cache.lastUpdated || 0)) {
                this.cache.lastUpdated = data.lastUpdated;
            }
            return data;
        } catch (err) {
            // console.warn('Failed to sync with server:', err);
            throw err; // Re-throw to handle offline scenarios
        }
    }

    setOptions(opts = {}) {
        if (typeof opts.apiBaseUrl === 'string') this.apiBaseUrl = opts.apiBaseUrl;
        if (typeof opts.enablePersistence === 'boolean') this.enablePersistence = opts.enablePersistence;
        if (opts.storageKey) this.storageKey = opts.storageKey;
        this._saveToStorage();
    }

    // --- Improved Public API with batch fetching ---
    // Fetch all data in one go if expired or forced
    async fetchAllData(forceRefresh = false) {
        if (!forceRefresh && !this._isCacheExpired()) {
            return this.cache;
        }

        try {
            // Single endpoint for all data to avoid multiple requests
            const serverData = await this._syncToServer('GET', '/all-data');
            if (serverData) {
                this.cache = { ...this.cache, ...serverData };
                this._markUpdated();
                return this.cache;
            }
        } catch (err) {
            // Fallback to local/mock if offline
            console.warn('Using local cache due to network error');
        }

        // Mock/fallback data only if no cache
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
            this.cache.penalties = []; // Add mock if needed
            this.cache.bonuses = [];
            this._computeDashboard(); // Compute aggregates locally
        }
        this._markUpdated();
        return this.cache;
    }

    // Compute dashboard aggregates locally from cache
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

    async addEmployee(employeeData) {
        const newEmployee = {
            ...employeeData,
            id: Date.now(), // Temporary ID, server will assign real one
            penalties: employeeData.penalties || 0,
            bonuses: employeeData.bonuses || 0,
            totalPenalties: 0,
            totalBonuses: 0
        };
        this.cache.employees.push(newEmployee);
        this.cache.hours.push({ employeeId: newEmployee.id, regularHours: 0, overtime: 0, undertime: 0 });
        this._computeDashboard();
        this._markUpdated();

        try {
            const serverResp = await this._syncToServer('POST', '/employees', newEmployee);
            if (serverResp && serverResp.id) {
                // Update with server ID and data
                const idx = this.cache.employees.findIndex(e => e.id === newEmployee.id);
                if (idx !== -1) this.cache.employees[idx] = serverResp;
                // Update hours if needed
                await this.fetchAllData(true); // Force refresh after create
            }
        } catch (err) {}
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
        } catch (err) {}
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
        } catch (err) {}
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
        } catch (err) {}
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
        } catch (err) {}
    }

    clearCache() {
        this.cache = { dashboard: null, employees: [], hours: [], penalties: [], bonuses: [], lastUpdated: null };
        try { localStorage.removeItem(this.storageKey); } catch (e) {}
    }
}

// Initialize and expose the data cache
if (!window.dataCache) {
    window.dataCache = new DataCache();
}