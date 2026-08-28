#ifndef ERIRE_LICENSE_GUARD_H
#define ERIRE_LICENSE_GUARD_H

#include <stdbool.h>
#include <stddef.h>

typedef enum ErLicenseAppKind {
    ER_LICENSE_APP_CLI = 1,
    ER_LICENSE_APP_STUDIO = 2
} ErLicenseAppKind;

bool er_license_guard_require(ErLicenseAppKind app_kind, char *error, size_t error_size);
bool er_license_guard_verify_key(const char *product_key, const char *google_account, char *error, size_t error_size);

#endif
