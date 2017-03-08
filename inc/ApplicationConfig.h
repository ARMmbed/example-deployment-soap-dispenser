#ifndef _APPLICATION_CONFIG_H
#define _APPLICATION_CONFIG_H

#include "mDot.h"

#define CONFIG_NVM_OFFSET           0x1700

typedef struct {

    uint32_t presses_left;
    uint32_t tx_interval;
    bool     initialized;

} ApplicationConfigData;


class ApplicationConfig {
public:
    ApplicationConfig(mDot* dotInstance) : dot(dotInstance) {
        dot->nvmRead(CONFIG_NVM_OFFSET, &data, sizeof(ApplicationConfig));

        if (!data.initialized) {
            data.presses_left = 1000;
            data.tx_interval = 60;
            data.initialized = true;
        }
    }

    uint32_t get_presses_left() {
        return data.presses_left;
    }

    uint32_t get_tx_interval_s() {
        return data.tx_interval;
    }

    void decrease_presses_left() {
        data.presses_left--;
        persist();
    }

    void set_tx_interval_s(uint32_t tx_interval_s) {
        data.tx_interval = tx_interval_s;
        persist();
    }

private:
    void persist() {
        dot->nvmWrite(CONFIG_NVM_OFFSET, &data, sizeof(ApplicationConfig));
    }

    mDot* dot;
    ApplicationConfigData data;
};

#endif // _APPLICATION_CONFIG_H
