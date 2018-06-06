#ifndef _NRFLite_h_
#define _NRFLite_h_

#include <Arduino.h>
#include <nRF24L01.h>

class NRFLite {

  public:

    // Constructors
    // Optionally pass in an Arduino Serial or SoftwareSerial object for use throughout the library when debugging.
    // Use the debug and debugln DEFINES in NRFLite.cpp to use the serial object.
    NRFLite() {}
    NRFLite(Stream &serial) : _serial(&serial) {}
    
    enum Bitrates { BITRATE2MBPS, BITRATE1MBPS, BITRATE250KBPS };
    enum SendType { REQUIRE_ACK, NO_ACK, REQUIRE_SACK };
    
    // Methods for receivers and transmitters.
    // init       = Turns the radio on and puts it into receiving mode.  Returns 0 if it cannot communicate with the radio.
    //              Channel can be 0-125 and sets the exact frequency of the radio between 2400 - 2525 MHz.
    // initTwoPin = Same as init but with multiplexed MOSI/MISO and CE/CSN/SCK pins (only works on AVR architectures).
    //              Follow the 2-Pin Hookup Guide on https://github.com/dparson55/NRFLite
    //              Theory from http://nerdralph.blogspot.ca/2015/05/nrf24l01-control-with-2-mcu-pins-using.html
    //              Note the capacitor and resistor values from the blog's schematic are not used, instead use a 0.1uF capacitor, 
    //              1K resistor between CE/CSN and SCK, and 3K-6K resistor between MOSI and MISO.
    // readData   = Loads a received data packet or acknowledgment packet into the specified data parameter.
    // powerDown  = Power down the radio.  It only draws 900 nA in this state.  Turn the radio back on by calling one of the 
    //              'hasData' or 'send' methods.
    // printDetails = For debugging, it prints most radio registers if a serial object is provided in the constructor.
    uint8_t init(uint8_t radioId, uint8_t cePin, uint8_t csnPin, Bitrates bitrate = BITRATE2MBPS, uint8_t channel = 100);
#if defined(__AVR__)
    uint8_t initTwoPin(uint8_t radioId, uint8_t momiPin, uint8_t sckPin, Bitrates bitrate = BITRATE2MBPS, uint8_t channel = 100);
#endif
    void readData(void *data);
    void powerDown();
    void printDetails();

    // Methods for transmitters.
    // send = Sends a data packet and waits for success or failure.  The default REQUIRE_ACK sendType causes the radio
    //        to attempt sending the packet up to 16 times.  If successful a 1 is returned.  Optionally the NO_ACK sendType
    //        can be used to transmit the packet a single time without any acknowledgement.
    // hasAckData = Checks to see if an ACK data packet was received and returns its length.
    uint8_t send(uint8_t toRadioId, void *data, uint8_t length, SendType sendType = REQUIRE_ACK);
    uint8_t hasAckData();

    // Methods for receivers.
    // hasData    = Checks to see if a data packet has been received and returns its length.
    // addAckData = Enqueues an acknowledgment packet for sending back to a transmitter.  Whenever the transmitter sends the 
    //              next data packet, it will get this ACK packet back in the response.
    // removeAckData = Removes any acknowledgement packet loaded into the transmitter.
    uint8_t hasData(uint8_t usingInterrupts = 0);
    void addAckData(void *data, uint8_t length);
    void removeAckData();
    
    // Methods when using the radio's IRQ pin for interrupts.
    // Note that if interrupts are used, do not use the send and hasData functions.  Instead the functions below must be used.
    // startSend    = Start sending a data packet without waiting for it to complete.
    // whatHappened = Use this inside the interrupt handler to see what caused the interrupt.
    // hasDataISR   = Same as hasData(1) and is just for clarity.  It will greatly speed up the receive bitrate when
    //                CE and CSN share the same pin.
    void startSend(uint8_t toRadioId, void *data, uint8_t length, SendType sendType = REQUIRE_ACK); 
    void whatHappened(uint8_t &txOk, uint8_t &txFail, uint8_t &rxReady);
    uint8_t hasDataISR(); 
    
  private:

    const static uint8_t OFF_TO_POWERDOWN_MILLIS = 100; // Vcc > 1.9V power on reset time.
    const static uint8_t STANDBY_TO_RXTX_MODE_MICROS = 130; // Standby to RX or TX mode time.
    const static uint16_t POWERDOWN_TO_RXTX_MODE_MICROS = 4500 + STANDBY_TO_RXTX_MODE_MICROS;
    const static uint8_t CE_TRANSMISSION_MICROS = 11; // Time to initiate data transmission.

    enum SpiTransferType { READ_OPERATION, WRITE_OPERATION };

    Stream *_serial;
    uint8_t _cePin, _csnPin, _resetInterruptFlags;
    uint16_t _transmissionRetryWaitMicros, _maxHasDataIntervalMicros;
    uint32_t _microsSinceLastDataCheck;
    
    uint8_t getPipeOfFirstRxPacket();
    uint8_t getRxPacketLength();
    uint8_t prepForRx(uint8_t radioId, Bitrates bitrate, uint8_t channel);
    void prepForTx(uint8_t toRadioId, SendType sendType);
    uint8_t readRegister(uint8_t regName);
    void readRegister(uint8_t regName, void* data, uint8_t length);
    void writeRegister(uint8_t regName, uint8_t data);
    void writeRegister(uint8_t regName, void* data, uint8_t length);
    void spiTransfer(SpiTransferType transferType, uint8_t regName, void* data, uint8_t length);
    uint8_t usiTransfer(uint8_t data);    
    void printRegister(char name[], uint8_t regName);

    // 2-Pin Variables and Methods
    const static uint16_t CSN_DISCHARGE_MICROS = 500; // Determined by measuring time to discharge CSN on a 1MHz ATtiny using 0.1uF capacitor and 1K resistor.

    volatile uint8_t *_momi_PORT, *_momi_DDR, *_momi_PIN, *_sck_PORT;
    uint8_t _momi_MASK, _sck_MASK, _useTwoPinSpiTransfer;
    uint8_t twoPinTransfer(uint8_t data);

    // Software-based Acknowledgement Variables and Methods
    const static uint8_t SACK_TX_COMPLETE_MILLIS = 10; // Time to wait for TX to complete.
    const static uint8_t SACK_TX_TO_RX_MILLIS = SACK_TX_COMPLETE_MILLIS + 10; // Time for transmitter to become a receiver.
    const static uint8_t SACK_RX_WAIT_TIME_MILLIS = SACK_TX_TO_RX_MILLIS + 10; // Time to wait for reception of the SACK packet.

    uint8_t _radioId, _useSack, _lastPacketId, _ackDataPacket[32], _ackDataLength, _receivedDataPacket[32], _receivedDataLength;
    uint16_t _lastReceivedPacketHeader;

    uint8_t getRxPacketLengthAndSendSack();
    uint8_t sendAndWaitForSack(uint8_t toRadioId, void *data, uint8_t length);
};

#endif
