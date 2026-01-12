// modules/dataCache.js - Centralized local data cache with server synchronization, caching, offline mode, and event-based updates

class DataCache {
    constructor(options = {}) {
        this.storageKey = options.storageKey || 'hr_data_cache_v3'; // Updated version for schema changes
        this.cache = {
            dashboard: null,
            departments: [], // {id, depName, phoneNumber}
            posts: [], // {id, postName, baseSalary, departmentId}
            employees: [], // {id, firstName, lastName, patronymic, phoneNumber, workStatus, postId}
            salaries: [], // {id, employeeId, baseSalary, multiplier, agreeSalary, contractNu}
            timeRecords: [], // {id, employeeId, date, timeIn, timeOut}
            applicants: [], // {id, firstName, lastName, patronymic, phoneNumber, workStatus, postName, resume, salary} - Added based on requirements
            penalties: [], // {id, employeeId, reason, amount, date}
            bonuses: [], // {id, employeeId, note, amount, date}
            lastUpdated: null
        };

        this.apiBaseUrl = options.apiBaseUrl || '/api';
        this.enablePersistence = typeof options.enablePersistence === 'boolean' ? options.enablePersistence : true;
        this.isOfflineMode = false;

        this._loadFromStorage();
    }

    // Persistence methods
    _loadFromStorage() {
        if (!this.enablePersistence) return;
        try {
            const storedData = localStorage.getItem(this.storageKey);
            if (storedData) {
                const parsed = JSON.parse(storedData);
                this.cache = { ...this.cache, ...parsed };
            }
        } catch (error) {
            console.warn('Error loading cache from localStorage:', error);
        }
    }

    _saveToStorage() {
        if (!this.enablePersistence) return;
        try {
            localStorage.setItem(this.storageKey, JSON.stringify(this.cache));
        } catch (error) {
            console.warn('Error saving cache to localStorage:', error);
        }
    }

    _markUpdated() {
        this.cache.lastUpdated = new Date().toISOString();
        this._saveToStorage();
        // Dispatch event for UI updates (e.g., ABU uses this for interface refresh)
        window.dispatchEvent(new CustomEvent('dataCache:updated', { detail: { lastUpdated: this.cache.lastUpdated } }));
        // Automatically notify via notifications module if available
        if (window.notifications) {
            window.notifications.showSuccess('Данные успешно обновлены');
        }
    }

    _isCacheExpired() {
        if (!this.cache.lastUpdated) return true;
        const now = new Date();
        const lastUpdate = new Date(this.cache.lastUpdated);
        const diffMinutes = (now - lastUpdate) / (1000 * 60);
        return diffMinutes > 5; // Cache TTL: 5 minutes
    }

    // Network synchronization with offline fallback
    async _syncToServer(method, path, body = null, options = {}) {
        if (this.isOfflineMode) {
            console.warn('Offline mode: Operation queued or using local cache');
            throw new Error('Offline mode active');
        }

        try {
            const url = `${this.apiBaseUrl}${path}`;
            const headers = { 'Content-Type': 'application/json' };
            // Future auth placeholder
            // if (localStorage.getItem('authToken')) headers['Authorization'] = `Bearer ${localStorage.getItem('authToken')}`;

            const response = await fetch(url, {
                method,
                headers,
                body: body ? JSON.stringify(body) : undefined,
                ...options
            });

            if (!response.ok) {
                const errorText = await response.text();
                const errorObj = {
                    status: response.status,
                    statusText: response.statusText,
                    body: errorText
                };
                throw errorObj;
            }

            const data = await response.json();
            if (data.lastUpdated && new Date(data.lastUpdated) > new Date(this.cache.lastUpdated || 0)) {
                this.cache.lastUpdated = data.lastUpdated;
            }
            this.isOfflineMode = false;
            return data;
        } catch (error) {
            console.error('Network error:', error);
            this.isOfflineMode = true;
            // In offline, do not throw for read operations; for writes, queue if needed (future impl)
            throw error;
        }
    }

    isOffline() {
        return this.isOfflineMode;
    }

    setOptions(options = {}) {
        if (options.apiBaseUrl) this.apiBaseUrl = options.apiBaseUrl;
        if (typeof options.enablePersistence === 'boolean') this.enablePersistence = options.enablePersistence;
        if (options.storageKey) this.storageKey = options.storageKey;
        this._saveToStorage();
    }

    // Fetch all data with auto-sync
    async fetchAllData(forceRefresh = false) {
        if (!forceRefresh && !this._isCacheExpired() && this.cache.departments.length > 0) {
            return this.cache;
        }

        try {
            const serverData = await this._syncToServer('GET', '/all-data');
            Object.assign(this.cache, serverData);
            this._computeDashboard();
            this._markUpdated();
            return this.cache;
        } catch (error) {
            if (this.cache.departments.length === 0) {
                // Fallback mock data if cache empty
                this.cache.departments = [{ id: 1, depName: 'IT Department', phoneNumber: '+1234567890' }];
                this.cache.posts = [{ id: 1, postName: 'Software Developer', baseSalary: 50000, departmentId: 1 }];
                this.cache.employees = [{ id: 1, firstName: 'Jane', lastName: 'Smith', patronymic: '', phoneNumber: '+0987654321', workStatus: 'active', postId: 1 }];
                this.cache.salaries = [{ id: 1, employeeId: 1, baseSalary: 50000, multiplier: 1.2, agreeSalary: 60000, contractNu: 'CON-001' }];
                this.cache.timeRecords = [{ id: 1, employeeId: 1, date: '2026-01-10', timeIn: '08:00', timeOut: '17:00' }];
                this.cache.applicants = [{ id: 1, firstName: 'Applicant', lastName: 'One', patronymic: '', phoneNumber: '+1122334455', workStatus: 'pending', postName: 'Developer', resume: 'resume.txt', salary: 45000 }];
                this.cache.penalties = [];
                this.cache.bonuses = [];
                this._computeDashboard();
                this._markUpdated();
            }
            return this.cache;
        }
    }

    // Compute dashboard metrics
    _computeDashboard() {
        const activeEmployees = this.cache.employees.filter(emp => emp.workStatus === 'active' || emp.workStatus === 'hired');
        let totalUndertime = 0;
        activeEmployees.forEach(emp => {
            const records = this.cache.timeRecords.filter(rec => rec.employeeId === emp.id);
            records.forEach(rec => {
                const inTime = new Date(`1970-01-01T${rec.timeIn}:00`);
                const outTime = new Date(`1970-01-01T${rec.timeOut}:00`);
                const hoursWorked = (outTime - inTime) / (1000 * 60 * 60);
                if (hoursWorked < 8) totalUndertime += (8 - hoursWorked);
            });
        });

        this.cache.dashboard = {
            employeeCount: activeEmployees.length,
            applicantCount: this.cache.applicants.length,
            penaltyCount: this.cache.penalties.length,
            bonusCount: this.cache.bonuses.length,
            totalUndertime,
            totalPenalties: this.cache.penalties.reduce((sum, p) => sum + (p.amount || 0), 0),
            totalBonuses: this.cache.bonuses.reduce((sum, b) => sum + (b.amount || 0), 0)
        };
    }

    async getDashboardData() {
        await this.fetchAllData();
        return this.cache.dashboard;
    }

    // Departments CRUD
    async getDepartments() {
        await this.fetchAllData();
        return this.cache.departments;
    }

    async addDepartment(data) {
        const payload = { depName: data.depName.trim(), phoneNumber: data.phoneNumber.trim() };
        const tempId = Date.now();
        const newDept = { id: tempId, ...payload };
        this.cache.departments.push(newDept);
        this._markUpdated();

        try {
            const serverData = await this._syncToServer('POST', '/departments', payload);
            const index = this.cache.departments.findIndex(d => d.id === tempId);
            if (index !== -1) this.cache.departments[index] = serverData;
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return newDept;
    }

    async updateDepartment(id, changes) {
        const dept = this.cache.departments.find(d => d.id === id);
        if (!dept) throw new Error('Department not found');
        Object.assign(dept, changes);
        this._markUpdated();

        try {
            await this._syncToServer('PUT', `/departments/${id}`, changes);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return dept;
    }

    async deleteDepartment(id) {
        const index = this.cache.departments.findIndex(d => d.id === id);
        if (index === -1) throw new Error('Department not found');
        this.cache.departments.splice(index, 1);
        this._markUpdated();

        try {
            await this._syncToServer('DELETE', `/departments/${id}`);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    // Posts CRUD
    async getPosts() {
        await this.fetchAllData();
        return this.cache.posts;
    }

    async addPost(data) {
        const payload = { postName: data.postName.trim(), baseSalary: Number(data.baseSalary), departmentId: Number(data.departmentId) };
        const tempId = Date.now();
        const newPost = { id: tempId, ...payload };
        this.cache.posts.push(newPost);
        this._markUpdated();

        try {
            const serverData = await this._syncToServer('POST', '/posts', payload);
            const index = this.cache.posts.findIndex(p => p.id === tempId);
            if (index !== -1) this.cache.posts[index] = serverData;
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return newPost;
    }

    async updatePost(id, changes) {
        const post = this.cache.posts.find(p => p.id === id);
        if (!post) throw new Error('Post not found');
        Object.assign(post, changes);
        this._markUpdated();

        try {
            await this._syncToServer('PUT', `/posts/${id}`, changes);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return post;
    }

    async deletePost(id) {
        const index = this.cache.posts.findIndex(p => p.id === id);
        if (index === -1) throw new Error('Post not found');
        this.cache.posts.splice(index, 1);
        this._markUpdated();

        try {
            await this._syncToServer('DELETE', `/posts/${id}`);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    // Employees CRUD
    async getEmployees() {
        await this.fetchAllData();
        return this.cache.employees;
    }

    async addEmployee(data) {
        const payload = {
            firstName: data.firstName.trim(),
            lastName: data.lastName.trim(),
            patronymic: data.patronymic?.trim() || '',
            phoneNumber: data.phoneNumber.trim(),
            workStatus: data.workStatus,
            postId: Number(data.postId)
        };
        const tempId = Date.now();
        const newEmployee = { id: tempId, ...payload };
        this.cache.employees.push(newEmployee);
        this._computeDashboard();
        this._markUpdated();

        try {
            const serverData = await this._syncToServer('POST', '/employees', payload);
            const index = this.cache.employees.findIndex(e => e.id === tempId);
            if (index !== -1) this.cache.employees[index] = serverData;
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return newEmployee;
    }

    async updateEmployee(id, changes) {
        const employee = this.cache.employees.find(e => e.id === id);
        if (!employee) throw new Error('Employee not found');
        Object.assign(employee, changes);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('PUT', `/employees/${id}`, changes);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return employee;
    }

    async deleteEmployee(id) {
        const index = this.cache.employees.findIndex(e => e.id === id);
        if (index === -1) throw new Error('Employee not found');
        this.cache.employees.splice(index, 1);
        // Cascade delete related
        this.cache.salaries = this.cache.salaries.filter(s => s.employeeId !== id);
        this.cache.timeRecords = this.cache.timeRecords.filter(t => t.employeeId !== id);
        this.cache.penalties = this.cache.penalties.filter(p => p.employeeId !== id);
        this.cache.bonuses = this.cache.bonuses.filter(b => b.employeeId !== id);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('DELETE', `/employees/${id}`);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    // Salaries CRUD
    async getSalaries() {
        await this.fetchAllData();
        return this.cache.salaries;
    }

    async getSalaryByEmployeeId(employeeId) {
        await this.fetchAllData();
        return this.cache.salaries.find(s => s.employeeId === employeeId);
    }

    async addSalary(data) {
        const payload = {
            employeeId: Number(data.employeeId),
            baseSalary: Number(data.baseSalary),
            multiplier: Number(data.multiplier || 1.0),
            agreeSalary: Number(data.agreeSalary),
            contractNu: data.contractNu.trim()
        };
        const tempId = Date.now();
        const newSalary = { id: tempId, ...payload };
        this.cache.salaries.push(newSalary);
        this._computeDashboard();
        this._markUpdated();

        try {
            const serverData = await this._syncToServer('POST', '/salaries', payload);
            const index = this.cache.salaries.findIndex(s => s.id === tempId);
            if (index !== -1) this.cache.salaries[index] = serverData;
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return newSalary;
    }

    async updateSalary(id, changes) {
        const salary = this.cache.salaries.find(s => s.id === id);
        if (!salary) throw new Error('Salary not found');
        Object.assign(salary, changes);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('PUT', `/salaries/${id}`, changes);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return salary;
    }

    async deleteSalary(id) {
        const index = this.cache.salaries.findIndex(s => s.id === id);
        if (index === -1) throw new Error('Salary not found');
        this.cache.salaries.splice(index, 1);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('DELETE', `/salaries/${id}`);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    // Time Records CRUD
    async getTimeRecords() {
        await this.fetchAllData();
        return this.cache.timeRecords;
    }

    async getTimeRecordsByEmployeeId(employeeId) {
        await this.fetchAllData();
        return this.cache.timeRecords.filter(t => t.employeeId === employeeId);
    }

    async addTimeRecord(data) {
        const payload = {
            employeeId: Number(data.employeeId),
            date: data.date,
            timeIn: data.timeIn,
            timeOut: data.timeOut
        };
        const tempId = Date.now();
        const newRecord = { id: tempId, ...payload };
        this.cache.timeRecords.push(newRecord);
        this._computeDashboard();
        this._markUpdated();

        try {
            const serverData = await this._syncToServer('POST', '/time-records', payload);
            const index = this.cache.timeRecords.findIndex(t => t.id === tempId);
            if (index !== -1) this.cache.timeRecords[index] = serverData;
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return newRecord;
    }

    async updateTimeRecord(id, changes) {
        const record = this.cache.timeRecords.find(t => t.id === id);
        if (!record) throw new Error('Time record not found');
        Object.assign(record, changes);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('PUT', `/time-records/${id}`, changes);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return record;
    }

    async deleteTimeRecord(id) {
        const index = this.cache.timeRecords.findIndex(t => t.id === id);
        if (index === -1) throw new Error('Time record not found');
        this.cache.timeRecords.splice(index, 1);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('DELETE', `/time-records/${id}`);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    // Applicants CRUD (added based on diagram requirements)
    async getApplicants() {
        await this.fetchAllData();
        return this.cache.applicants;
    }

    async addApplicant(data) {
        const payload = {
            firstName: data.firstName.trim(),
            lastName: data.lastName.trim(),
            patronymic: data.patronymic?.trim() || '',
            phoneNumber: data.phoneNumber.trim(),
            workStatus: data.workStatus || 'pending',
            postName: data.postName.trim(),
            resume: data.resume,
            salary: Number(data.salary)
        };
        const tempId = Date.now();
        const newApplicant = { id: tempId, ...payload };
        this.cache.applicants.push(newApplicant);
        this._computeDashboard();
        this._markUpdated();

        try {
            const serverData = await this._syncToServer('POST', '/applicants', payload);
            const index = this.cache.applicants.findIndex(a => a.id === tempId);
            if (index !== -1) this.cache.applicants[index] = serverData;
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return newApplicant;
    }

    async updateApplicant(id, changes) {
        const applicant = this.cache.applicants.find(a => a.id === id);
        if (!applicant) throw new Error('Applicant not found');
        Object.assign(applicant, changes);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('PUT', `/applicants/${id}`, changes);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return applicant;
    }

    async deleteApplicant(id) {
        const index = this.cache.applicants.findIndex(a => a.id === id);
        if (index === -1) throw new Error('Applicant not found');
        this.cache.applicants.splice(index, 1);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('DELETE', `/applicants/${id}`);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
    }

    // Penalties and Bonuses
    async addPenalty(employeeId, data) {
        const penalty = { id: Date.now(), employeeId: Number(employeeId), ...data, date: new Date().toISOString() };
        this.cache.penalties.push(penalty);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('POST', `/employees/${employeeId}/penalties`, data);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return penalty;
    }

    async addBonus(employeeId, data) {
        const bonus = { id: Date.now(), employeeId: Number(employeeId), ...data, date: new Date().toISOString() };
        this.cache.bonuses.push(bonus);
        this._computeDashboard();
        this._markUpdated();

        try {
            await this._syncToServer('POST', `/employees/${employeeId}/bonuses`, data);
            await this.fetchAllData(true);
        } catch (error) {
            if (!this.isOfflineMode) throw error;
        }
        return bonus;
    }

    // Aggregated hours calculation
    async getHoursForEmployee(employeeId) {
        const records = await this.getTimeRecordsByEmployeeId(employeeId);
        let regular = 0, overtime = 0, undertime = 0;
        records.forEach(rec => {
            const inTime = new Date(`1970-01-01T${rec.timeIn}:00`);
            const outTime = new Date(`1970-01-01T${rec.timeOut}:00`);
            const workedHours = (outTime - inTime) / (1000 * 60 * 60);
            if (workedHours > 8) {
                regular += 8;
                overtime += workedHours - 8;
            } else if (workedHours < 8) {
                regular += workedHours;
                undertime += 8 - workedHours;
            } else {
                regular += 8;
            }
        });
        return { employeeId, regularHours: regular, overtime, undertime };
    }

    clearCache() {
        this.cache = {
            dashboard: null,
            departments: [],
            posts: [],
            employees: [],
            salaries: [],
            timeRecords: [],
            applicants: [],
            penalties: [],
            bonuses: [],
            lastUpdated: null
        };
        if (this.enablePersistence) localStorage.removeItem(this.storageKey);
        this.isOfflineMode = false;
        this._markUpdated();
    }
}

// Global instance
if (!window.dataCache) {
    window.dataCache = new DataCache();
}