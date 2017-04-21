#include "dot_util.h"
#include "RadioEvent.h"
#include "ApplicationConfig.h"

static uint8_t network_id[] = { 0xBE, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x03, 0x93 };
static uint8_t network_key[] = { 0x48, 0x4A, 0x12, 0x7D, 0x98, 0xAF, 0x14, 0x36, 0xDB, 0xF3, 0xBC, 0x8C, 0xF6, 0x00, 0x44, 0x48};
static uint8_t frequency_sub_band = 0;
static bool public_network = true;
static bool ack = 0;

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

static bool interrupt = false;
static uint32_t sleep_until = 0;

void counter_dec() {
    interrupt = true;
    config->decrease_presses_left();
}

void counter_reset() {
    interrupt = true;
    config->reset_presses_left();
}

void low_battery() {
    interrupt = true;
    config->alert_low_battery();
}

void stable_battery() {
    interrupt = true;
    config->alert_stable_battery();
}

int main() {
    pc.baud(115200);
    
    InterruptIn dispense(GPIO1);
    dispense.fall(&counter_dec);
    
    InterruptIn lowBat(GPIO2);
    lowBat.fall(&low_battery);
    
    InterruptIn resetCounter(GPIO3);
    resetCounter.fall(&counter_reset);
    


    // gets disabled automatically when going to sleep and restored when waking up
    DigitalOut led(GPIO0, 1);

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
        if (interrupt) {
            interrupt = false;

            // we only care about waking up from RTC, so go back to sleep asap
            uint32_t sleep_time = sleep_until - time(NULL);
            logInfo("Woke from interrupt, going back to sleep for %d seconds", sleep_time);
            sleep_wake_rtc_or_interrupt(sleep_time, deep_sleep);
            continue;
        }

        std::vector<uint8_t> tx_data;

        // join network if not joined
        if (!dot->getNetworkJoinStatus()) {
            join_network();
        }

        uint32_t presses_left = config->get_presses_left();

        tx_data.push_back((presses_left >> 8) & 0xff);
        tx_data.push_back(presses_left & 0xff);
        
        if (resetCounter==0) {
            config->reset_presses_left();
            logInfo("Replenished gel");
        }
        
        //To clear the lowBat status catching high interrupt will not work, simply check status of the GPIO on runtime and resets it here.
        if (lowBat==1) {
            stable_battery();
            tx_data.push_back(0x00);
            logInfo("Battery voltage healthy");
        }
        
//        else {
//            low_battery();
//            tx_data.push_back(0x01);
//            logInfo("ALERT! low battery");
//        }

        
        //As this is interrupt drive above, simply get status of battery, will only push data is low battery
        if (config->get_battery_status()) {
            tx_data.push_back(0x01);
            logInfo("ALERT! low battery");
            
        }
        
        logInfo("Sending presses %d", presses_left);

        send_data(tx_data);
        
        

        // if going into deepsleep mode, save the session so we don't need to join again after waking up
        // not necessary if going into sleep mode since RAM is retained
        if (deep_sleep) {
            logInfo("saving network session to NVM");
            dot->saveNetworkSession();
        }

        uint32_t sleep_time = calculate_actual_sleep_time(config->get_tx_interval_s());
        sleep_until = time(NULL) + sleep_time;

        // ONLY ONE of the three functions below should be uncommented depending on the desired wakeup method
        //sleep_wake_rtc_only(deep_sleep);
        //sleep_wake_interrupt_only(deep_sleep);
        sleep_wake_rtc_or_interrupt(sleep_time, deep_sleep);
    }

    return 0;
}
