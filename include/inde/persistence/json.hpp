#pragma once
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace inde::persistence::json {

class Error : public std::runtime_error {
public:
    Error(std::string message, std::size_t line, std::size_t column);
    [[nodiscard]] std::size_t line() const noexcept { return line_; }
    [[nodiscard]] std::size_t column() const noexcept { return column_; }
private:
    std::size_t line_, column_;
};

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object>;

    Value() : data_(nullptr) {}
    Value(std::nullptr_t) : data_(nullptr) {}
    Value(bool value) : data_(value) {}
    Value(int value) : data_(static_cast<std::int64_t>(value)) {}
    Value(std::int64_t value) : data_(value) {}
    Value(double value) : data_(value) {}
    Value(std::string value) : data_(std::move(value)) {}
    Value(const char* value) : data_(std::string(value)) {}
    Value(Array value) : data_(std::move(value)) {}
    Value(Object value) : data_(std::move(value)) {}

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] std::int64_t as_integer() const;
    [[nodiscard]] bool as_boolean() const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;
    [[nodiscard]] const Value& at(std::string_view key) const;
    [[nodiscard]] const Storage& storage() const noexcept { return data_; }
private:
    Storage data_;
};

[[nodiscard]] Value parse(std::string_view source);
[[nodiscard]] std::string serialize(const Value& value, bool pretty = true);

} // namespace inde::persistence::json
