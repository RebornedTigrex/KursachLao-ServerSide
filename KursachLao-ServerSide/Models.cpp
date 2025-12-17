#include "Models.h"
#include <boost/json.hpp>

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Employee const& e) {
    jv = {
        {"id", e.id},
        {"fullname", e.fullname},
        {"status", e.status},
        {"salary", e.salary},
        {"penalties", e.penalties},
        {"bonuses", e.bonuses},
        {"totalPenalties", e.total_penalties},
        {"totalBonuses", e.total_bonuses}
    };
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Hours const& h) {
    jv = {
        {"employeeId", h.employee_id},
        {"regularHours", h.regular_hours},
        {"overtime", h.overtime},
        {"undertime", h.undertime}
    };
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Penalty const& p) {
    jv = {
        {"id", p.id},
        {"employeeId", p.employee_id},
        {"reason", p.reason},
        {"amount", p.amount},
        {"date", p.date}
    };
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Bonus const& b) {
    jv = {
        {"id", b.id},
        {"employeeId", b.employee_id},
        {"note", b.note},
        {"amount", b.amount},
        {"date", b.date}
    };
}

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Dashboard const& d) {
    jv = {
        {"penalties", d.penalties},
        {"bonuses", d.bonuses},
        {"undertime", d.undertime}
    };
}

// value_to (from_json)
Employee tag_invoke(boost::json::value_to_tag<Employee>, boost::json::value const& jv) {
    auto& obj = jv.as_object();
    Employee e;
    e.fullname = boost::json::value_to<std::string>(obj.at("fullname"));
    e.status = boost::json::value_to<std::string>(obj.at("status"));
    e.salary = boost::json::value_to<double>(obj.at("salary"));
    return e;
}

Hours tag_invoke(boost::json::value_to_tag<Hours>, boost::json::value const& jv) {
    auto& obj = jv.as_object();
    Hours h;
    h.employee_id = boost::json::value_to<int>(obj.at("employeeId"));
    h.regular_hours = obj.if_contains("regularHours") ? boost::json::value_to<double>(obj.at("regularHours")) : 0.0;
    h.overtime = obj.if_contains("overtime") ? boost::json::value_to<double>(obj.at("overtime")) : 0.0;
    h.undertime = obj.if_contains("undertime") ? boost::json::value_to<double>(obj.at("undertime")) : 0.0;
    return h;
}

Penalty tag_invoke(boost::json::value_to_tag<Penalty>, boost::json::value const& jv) {
    auto& obj = jv.as_object();
    Penalty p;
    p.employee_id = boost::json::value_to<int>(obj.at("employeeId"));
    p.reason = boost::json::value_to<std::string>(obj.at("reason"));
    p.amount = boost::json::value_to<double>(obj.at("amount"));
    return p;
}

Bonus tag_invoke(boost::json::value_to_tag<Bonus>, boost::json::value const& jv) {
    auto& obj = jv.as_object();
    Bonus b;
    b.employee_id = boost::json::value_to<int>(obj.at("employeeId"));
    b.note = boost::json::value_to<std::string>(obj.at("note"));
    b.amount = boost::json::value_to<double>(obj.at("amount"));
    return b;
}