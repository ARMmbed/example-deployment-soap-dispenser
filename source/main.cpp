#include "dot_util.h"
#include "RadioEvent.h"
#include "ApplicationConfig.h"

static uint8_t network_id[] = { 0xBE, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x03, 0x93 };
static uint8_t network_key[] = { 0x31, 0x39, 0x64, 0xC9, 0x25, 0x4D, 0x29, 0xFE, 0x3D, 0xE5, 0x59, 0xC0, 0xDB, 0xDE, 0x05, 0xF8 };
static uint8_t frequency_sub_band = 0;
static bool public_network = true;
static uint8_t ack = 0;

// deepsleep consumes slightly less current than sleep
// in sleep mode, IO state is maintained, RAM is retained, and application will resume after waking up
// in deepsleep mode, IOs float, RAM is lost, and application will start from beginning after waking up
// if deep_sleep == true, device will enter deepsleep mode
static bool deep_sleep = false;

mDot* dot;
ApplicationConfig* config;

Serial pc(USBTX, USBRX);

#if defined(TARGET_XDOT_L151CC)
I2C i2c(I2C_SDA, I2C_SCL);
ISL29011 lux(i2c);
#else
AnalogIn lux(XBEE_AD0);
#endif

int main() {
    pc.baud(115200);

    mts::MTSLog::setLogLevel(mts::MTSLog::TRACE_LEVEL);

    dot = mDot::getInstance();
    config = new ApplicationConfig(dot);

    // Custom event handler for automatically displaying RX data
    RadioEvent events(config);

    // attach the custom events handler
    dot->setEvents(&events);

    if (!dot->getStandbyFlag()) {
        logInfo("mbed-os library version: %d", MBED_LIBRARY_VERSION);

        // If we start up (not from standby), and the interval is over 60s we'll set it back to 60s.
        // Not sure if we actually want this...
        if (config->get_tx_interval_s() > 60) {
            config->set_tx_interval_s(60);
        }

        logInfo("configuration: presses_left=%d, tx_interval=%d", config->get_presses_left(), config->get_tx_interval_s());

        // start from a well-known state
        logInfo("defaulting Dot configuration");
        dot->resetConfig();
        dot->resetNetworkSession();

        // make sure library logging is turned on
        dot->setLogLevel(mts::MTSLog::INFO_LEVEL);

        // update configuration if necessary
        if (dot->getJoinMode() != mDot::OTA) {
            logInfo("changing network join mode to OTA");
            if (dot->setJoinMode(mDot::OTA) != mDot::MDOT_OK) {
                logError("failed to set network join mode to OTA");
            }
        }
        update_ota_config_id_key(network_id, network_key, frequency_sub_band, public_network, ack);

        // configure network link checks
        // network link checks are a good alternative to requiring the gateway to ACK every packet and should allow a single gateway to handle more Dots
        // check the link every count packets
        // declare the Dot disconnected after threshold failed link checks
        // for count = 3 and threshold = 5, the Dot will be considered disconnected after 15 missed packets in a row
        update_network_link_check_config(3, 5);

        logInfo("enabling ADR");
        if (dot->setAdr(true) != mDot::MDOT_OK) {
            logError("failed to enable ADR");
        }

        // Start in SF_10, and then ADR will find the most applicable datarate
        logInfo("setting TX datarate to SF_10");
        if (dot->setTxDataRate(mDot::SF_10) != mDot::MDOT_OK) {
            logError("failed to set TX datarate");
        }

        // save changes to configuration
        logInfo("saving configuration");
        if (!dot->saveConfig()) {
            logError("failed to save configuration");
        }

        // display configuration
        display_config();
    } else {
        logInfo("restoring network session from NVM");
        dot->restoreNetworkSession();
    }

    while (true) {
        std::vector<uint8_t> tx_data;

        // join network if not joined
        if (!dot->getNetworkJoinStatus()) {
            join_network();
        }

        uint32_t presses_left = config->get_presses_left();

        tx_data.push_back((presses_left >> 8) & 0xff);
        tx_data.push_back(presses_left & 0xff);

        send_data(tx_data);

        // if going into deepsleep mode, save the session so we don't need to join again after waking up
        // not necessary if going into sleep mode since RAM is retained
        if (deep_sleep) {
            logInfo("saving network session to NVM");
            dot->saveNetworkSession();
        }

        // ONLY ONE of the three functions below should be uncommented depending on the desired wakeup method
        //sleep_wake_rtc_only(deep_sleep);
        //sleep_wake_interrupt_only(deep_sleep);
        sleep_wake_rtc_or_interrupt(config->get_tx_interval_s() * 1000, deep_sleep);
    }

    return 0;
}
