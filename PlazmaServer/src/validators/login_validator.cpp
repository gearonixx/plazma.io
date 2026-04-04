#include "login_validator.hpp"
#include "utils/errors.hpp"

namespace real_medium::validator {

void validate(const handlers::TelegramLoginDTO& dto) {
    if (dto.first_name.empty()) {
        throw utils::error::ValidationException("first_name", "Field is missing");
    }

    if (dto.phone_number.empty()) {
        throw utils::error::ValidationException("phone_number", "Field is missing");
    }

    if (dto.user_id <= 0) {
        throw utils::error::ValidationException("user_id", "Invalid field");
    }
}

}  // namespace real_medium::validator
