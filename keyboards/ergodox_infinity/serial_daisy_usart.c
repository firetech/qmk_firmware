#include "quantum.h"
#include "serial.h"
#include "print.h"

#include <ch.h>
#include <hal.h>

#ifndef SELECT_SOFT_SERIAL_SPEED
#    define SELECT_SOFT_SERIAL_SPEED 1
#endif

#ifdef SERIAL_USART_SPEED
// Allow advanced users to directly set SERIAL_USART_SPEED
#elif SELECT_SOFT_SERIAL_SPEED == 0
#    define SERIAL_USART_SPEED 460800
#elif SELECT_SOFT_SERIAL_SPEED == 1
#    define SERIAL_USART_SPEED 230400
#elif SELECT_SOFT_SERIAL_SPEED == 2
#    define SERIAL_USART_SPEED 115200
#elif SELECT_SOFT_SERIAL_SPEED == 3
#    define SERIAL_USART_SPEED 57600
#elif SELECT_SOFT_SERIAL_SPEED == 4
#    define SERIAL_USART_SPEED 38400
#elif SELECT_SOFT_SERIAL_SPEED == 5
#    define SERIAL_USART_SPEED 19200
#else
#    error invalid SELECT_SOFT_SERIAL_SPEED value
#endif

#ifndef SERIAL_USART_TIMEOUT
#    define SERIAL_USART_TIMEOUT 50
#endif

#ifndef SERIAL_USART_CONNECTION_CHECK_TIMEOUT
#    define SERIAL_USART_CONNECTION_CHECK_TIMEOUT (SERIAL_USART_TIMEOUT * 5)
#endif

#define HANDSHAKE_MAGIC 7

static SerialDriver *driver;

static inline void sdClear(SerialDriver* driver) {
    while (sdGetTimeout(driver, TIME_IMMEDIATE) != MSG_TIMEOUT) {
        // Do nothing with the data
    }
}

static SerialConfig sdcfg = {
    (SERIAL_USART_SPEED),  // speed - mandatory
};

void handle_soft_serial_slave(void);

/*
 * This thread runs on the slave and responds to transactions initiated
 * by the master
 */
static THD_WORKING_AREA(waSlaveThread, 2048);
static THD_FUNCTION(SlaveThread, arg) {
    (void)arg;
    chRegSetThreadName("slave_transport");

    while (true) {
        handle_soft_serial_slave();
    }
}

void usart_master_init(void) {
    PORTA->PCR[1] = PORTx_PCRn_PE | PORTx_PCRn_PS | PORTx_PCRn_PFE | PORTx_PCRn_MUX(2);
    PORTA->PCR[2] = PORTx_PCRn_DSE | PORTx_PCRn_SRE | PORTx_PCRn_MUX(2);

    driver = &SD1;
    sdStart(driver, &sdcfg);
}

void usart_slave_init(void) {
    PORTE->PCR[0] = PORTx_PCRn_PE | PORTx_PCRn_PS | PORTx_PCRn_PFE | PORTx_PCRn_MUX(3);
    PORTE->PCR[1] = PORTx_PCRn_DSE | PORTx_PCRn_SRE | PORTx_PCRn_MUX(3);

    driver = &SD2;
    sdStart(driver, &sdcfg);

    // Start transport thread
    chThdCreateStatic(waSlaveThread, sizeof(waSlaveThread), HIGHPRIO, SlaveThread, NULL);
}

void soft_serial_initiator_init(void) { usart_master_init(); }

void soft_serial_target_init(void) { usart_slave_init(); }

void handle_soft_serial_slave(void) {
    uint8_t                   sstd_index = sdGet(driver);  // first chunk is always transaction id
    split_transaction_desc_t* trans      = &split_transaction_table[sstd_index];

    // Always write back the sstd_index as part of a basic handshake
    sstd_index ^= HANDSHAKE_MAGIC;
    sdWrite(driver, &sstd_index, sizeof(sstd_index));

    if (trans->initiator2target_buffer_size) {
        sdRead(driver, split_trans_initiator2target_buffer(trans), trans->initiator2target_buffer_size);
    }

    // Allow any slave processing to occur
    if (trans->slave_callback) {
        trans->slave_callback(trans->initiator2target_buffer_size, split_trans_initiator2target_buffer(trans), trans->target2initiator_buffer_size, split_trans_target2initiator_buffer(trans));
    }

    if (trans->target2initiator_buffer_size) {
        sdWrite(driver, split_trans_target2initiator_buffer(trans), trans->target2initiator_buffer_size);
    }

    if (trans->status) {
        *trans->status = TRANSACTION_ACCEPTED;
    }
}

/////////
//  start transaction by initiator
//
// int  soft_serial_transaction(int sstd_index)
//
// Returns:
//    TRANSACTION_END
//    TRANSACTION_NO_RESPONSE
//    TRANSACTION_DATA_ERROR
int soft_serial_transaction(int index) {
    uint8_t sstd_index = index;

    if (sstd_index > NUM_TOTAL_TRANSACTIONS) return TRANSACTION_TYPE_ERROR;
    split_transaction_desc_t* trans = &split_transaction_table[sstd_index];
    msg_t                     res   = 0;

    if (!trans->status) return TRANSACTION_TYPE_ERROR;  // not registered

    sdClear(driver);

    // Throttle transaction attempts if no slave has been found yet
    // Without this, the keyboard becomes unusable without a slave connected
    static bool serial_connected = false;
    static uint16_t connection_check_timer = 0;
    if (!serial_connected && timer_elapsed(connection_check_timer) < SERIAL_USART_CONNECTION_CHECK_TIMEOUT) {
        return TRANSACTION_NO_RESPONSE;
    }
    connection_check_timer = timer_read();

    // First chunk is always transaction id
    sdWriteTimeout(driver, &sstd_index, sizeof(sstd_index), TIME_MS2I(SERIAL_USART_TIMEOUT));

    uint8_t sstd_index_shake = 0xFF;

    // Which we always read back first so that we can error out correctly
    // Without the read, write only transactions *always* succeed, even during the boot process where the slave is not ready
    res = sdReadTimeout(driver, &sstd_index_shake, sizeof(sstd_index_shake), TIME_MS2I(SERIAL_USART_TIMEOUT));
    if (res < 0 || (sstd_index_shake != (sstd_index ^ HANDSHAKE_MAGIC))) {
        serial_connected = false;
        dprintf("serial::usart_shake NO_RESPONSE\n");
        return TRANSACTION_NO_RESPONSE;
    } else if (!serial_connected) {
        dprintf("serial::usart_shake CONNECTED\n");
    }
    serial_connected = true;

    if (trans->initiator2target_buffer_size) {
        res = sdWriteTimeout(driver, split_trans_initiator2target_buffer(trans), trans->initiator2target_buffer_size, TIME_MS2I(SERIAL_USART_TIMEOUT));
        if (res < 0) {
            dprintf("serial::usart_transmit NO_RESPONSE\n");
            return TRANSACTION_NO_RESPONSE;
        }
    }

    if (trans->target2initiator_buffer_size) {
        res = sdReadTimeout(driver, split_trans_target2initiator_buffer(trans), trans->target2initiator_buffer_size, TIME_MS2I(SERIAL_USART_TIMEOUT));
        if (res < 0) {
            dprintf("serial::usart_receive NO_RESPONSE\n");
            return TRANSACTION_NO_RESPONSE;
        }
    }

    return TRANSACTION_END;
}
