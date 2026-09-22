#ifndef LAUNCHER_CONTRACT_H
#define LAUNCHER_CONTRACT_H

#include "esp_err.h"

esp_err_t launcher_contract_mark_valid(void);
esp_err_t launcher_contract_return_to_factory(void);

#endif
