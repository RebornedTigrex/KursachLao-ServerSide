#pragma once

#include "BaseModule.h"
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <pqxx/pqxx>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

class DatabaseModule : public BaseModule {
private:
    std::string db_connection_string_;

    boost::asio::io_context& io_context_;

    std::unique_ptr<pqxx::connection> conn_;
    std::atomic<bool> db_ready_{ false };

    // SQL-скрипт создания схемы
    const std::string init_schema_sql_ = R"(
        CREATE TABLE IF NOT EXISTS employees (
            id SERIAL PRIMARY KEY,
            fullname TEXT NOT NULL,
            status TEXT NOT NULL CHECK (status IN ('hired', 'fired', 'interview')),
            salary NUMERIC(12,2) NOT NULL DEFAULT 0,
            penalties_count INTEGER DEFAULT 0,
            bonuses_count INTEGER DEFAULT 0,
            total_penalties NUMERIC(12,2) DEFAULT 0,
            total_bonuses NUMERIC(12,2) DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS work_hours (
            employee_id INTEGER PRIMARY KEY REFERENCES employees(id) ON DELETE CASCADE,
            regular_hours NUMERIC(8,2) DEFAULT 0,
            overtime NUMERIC(8,2) DEFAULT 0,
            undertime NUMERIC(8,2) DEFAULT 0,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS penalties (
            id SERIAL PRIMARY KEY,
            employee_id INTEGER REFERENCES employees(id) ON DELETE CASCADE,
            reason TEXT NOT NULL,
            amount NUMERIC(12,2) NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS bonuses (
            id SERIAL PRIMARY KEY,
            employee_id INTEGER REFERENCES employees(id) ON DELETE CASCADE,
            note TEXT NOT NULL,
            amount NUMERIC(12,2) NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        -- Триггеры для автоматического обновления счётчиков
        CREATE OR REPLACE FUNCTION update_employee_penalties() RETURNS TRIGGER AS $$
        BEGIN
            UPDATE employees
            SET penalties_count = penalties_count + 1,
                total_penalties = total_penalties + NEW.amount,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = NEW.employee_id;
            RETURN NEW;
        END;
        $$ LANGUAGE plpgsql;

        DROP TRIGGER IF EXISTS trg_penalty_insert ON penalties;
        CREATE TRIGGER trg_penalty_insert
            AFTER INSERT ON penalties
            FOR EACH ROW
            EXECUTE FUNCTION update_employee_penalties();

        CREATE OR REPLACE FUNCTION update_employee_bonuses() RETURNS TRIGGER AS $$
        BEGIN
            UPDATE employees
            SET bonuses_count = bonuses_count + 1,
                total_bonuses = total_bonuses + NEW.amount,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = NEW.employee_id;
            RETURN NEW;
        END;
        $$ LANGUAGE plpgsql;

        DROP TRIGGER IF EXISTS trg_bonus_insert ON bonuses;
        CREATE TRIGGER trg_bonus_insert
            AFTER INSERT ON bonuses
            FOR EACH ROW
            EXECUTE FUNCTION update_employee_bonuses();

        -- Автоматическое обновление updated_at в work_hours
        CREATE OR REPLACE FUNCTION update_hours_timestamp() RETURNS TRIGGER AS $$
        BEGIN
            NEW.updated_at = CURRENT_TIMESTAMP;
            RETURN NEW;
        END;
        $$ LANGUAGE plpgsql;

        DROP TRIGGER IF EXISTS trg_hours_update ON work_hours;
        CREATE TRIGGER trg_hours_update
            BEFORE UPDATE ON work_hours
            FOR EACH ROW
            EXECUTE FUNCTION update_hours_timestamp();
    )";

public:
    // Новый конструктор — принимает io_context по ссылке
    explicit DatabaseModule(
        boost::asio::io_context& ioc,
        const std::string& conn_str = "dbname=hr_db user=postgres password=postgres host=127.0.0.1 port=5432"
    );

    ~DatabaseModule() override;

    DatabaseModule(const DatabaseModule&) = delete;
    DatabaseModule& operator=(const DatabaseModule&) = delete;

    pqxx::connection* getConnection() {
        return db_ready_.load() ? conn_.get() : nullptr;
    }

    bool isDatabaseReady() const { return db_ready_.load(); }

protected:
    bool onInitialize() override;
    void onShutdown() override;

private:

    // Асинхронная инициализация базы
    void asyncInitializeDatabase();
};