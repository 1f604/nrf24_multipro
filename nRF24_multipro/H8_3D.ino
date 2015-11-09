
/*
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License.
 If not, see <http://www.gnu.org/licenses/>.
 */

#define H8_3D_PACKET_SIZE      20
#define H8_3D_RF_NUM_CHANNELS  4
#define H8_3D_ADDRESS_LENGTH   5
#define H8_3D_BIND_COUNT       1000
#define H8_3D_PACKET_PERIOD    1800 // Timeout for callback in uSec

enum {
    // flags going to packet[17]
    H8_3D_FLAG_FLIP     = 0x01,
    H8_3D_FLAG_RATE_MID = 0x02,
    H8_3D_FLAG_RATE_HIGH= 0x04,
    H8_3D_FLAG_HEADLESS = 0x10, // RTH + headless on H8, headless on JJRC H20
    H8_3D_FLAG_RTH      = 0x20, // 360� flip mode on H8 3D, RTH on JJRC H20
};

static uint8_t  H8_3D_txid[4] = { 0xae, 0xb8, 0x0b, 0x10}; // only 1 fixed id for now ...
static uint8_t  H8_3D_rf_chan;
static uint8_t  H8_3D_rf_channels[H8_3D_RF_NUM_CHANNELS] = { 0x0f, 0x19, 0x2f, 0x34};
static const uint8_t  H8_3D_rx_tx_addr[H8_3D_ADDRESS_LENGTH] = { 0xc4, 0x57, 0x09, 0x65, 0x21};

uint32_t process_H8_3D()
{
    uint32_t timeout = micros() + H8_3D_PACKET_PERIOD;
    H8_3D_send_packet(false);
    return timeout;
}

void H8_3D_bind()
{
    uint16_t counter = H8_3D_BIND_COUNT;
    while(counter--) {
        H8_3D_send_packet(true);
        delayMicroseconds(H8_3D_PACKET_PERIOD);
        digitalWrite(ledPin, counter & 0x10);
    }
    digitalWrite(ledPin, HIGH);
}    

uint8_t  H8_3D_checksum()
{
    uint8_t sum = packet[9];
    for (int i=10; i < H8_3D_PACKET_SIZE-1; i++)
        sum += packet[i];
    return sum;
}

void H8_3D_send_packet(uint8_t  bind)
{
    packet[0] = 0x13;
    memcpy(&packet[1], H8_3D_txid, 4);
    packet[8] = H8_3D_txid[0]+H8_3D_txid[1]+H8_3D_txid[2]+H8_3D_txid[3]; // txid checksum
    if (bind) {
        packet[5] = 0x00;
        packet[6] = 0x00;
        packet[7] = 0x01;
        memset( &packet[9], 0, 10);
    } else {
        packet[5] = H8_3D_rf_chan;
        packet[6] = 0x08;
        packet[7] = 0x03;
        packet[9] = map(ppm[THROTTLE], PPM_MIN, PPM_MAX, 0, 0xff); // throttle
    if( ppm[RUDDER] > PPM_MID)
        packet[10] = map(ppm[RUDDER], PPM_MID, PPM_MAX, 0, 0x3c); // rudder
    else
        packet[10] = map(ppm[RUDDER], PPM_MID, PPM_MIN, 0x80, 0xbc); // rudder
    packet[11] = map(ppm[ELEVATOR], PPM_MIN, PPM_MAX, 0x43, 0xbb); // elevator
    packet[12] = map(ppm[AILERON], PPM_MIN, PPM_MAX, 0xbb, 0x43); // aileron
    // neutral trims
    packet[13] = 0x20;
    packet[14] = 0x20;
    packet[15] = 0x20;
    packet[16] = 0x20;
    packet[17] = H8_3D_FLAG_RATE_HIGH
               | GET_FLAG( AUX2, H8_3D_FLAG_FLIP)
               | GET_FLAG( AUX5, H8_3D_FLAG_HEADLESS) // RTH+Headless on H8 3D
               | GET_FLAG( AUX6, H8_3D_FLAG_RTH); // 180/360 flip mode on H8 3D
    }
    packet[18] = 0x00;
    packet[19] = H8_3D_checksum(); // data checksum
    
    // Power on, TX mode, 2byte CRC
    // Why CRC0? xn297 does not interpret it - either 16-bit CRC or nothing
    XN297_Configure(_BV(NRF24L01_00_EN_CRC) | _BV(NRF24L01_00_CRCO) | _BV(NRF24L01_00_PWR_UP));

    NRF24L01_WriteReg(NRF24L01_05_RF_CH, bind ? H8_3D_rf_channels[0] : H8_3D_rf_channels[H8_3D_rf_chan++]);
    H8_3D_rf_chan %= sizeof(H8_3D_rf_channels);
    
    NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);
    NRF24L01_FlushTx();

    XN297_WritePayload(packet, H8_3D_PACKET_SIZE);
}

void H8_3D_init()
{
    NRF24L01_Initialize();
    NRF24L01_SetTxRxMode(TX_EN);

    XN297_SetTXAddr(H8_3D_rx_tx_addr, H8_3D_ADDRESS_LENGTH);

    NRF24L01_FlushTx();
    NRF24L01_FlushRx();
    NRF24L01_WriteReg(NRF24L01_01_EN_AA, 0x00);      // No Auto Acknowledgment on all data pipes
    NRF24L01_WriteReg(NRF24L01_02_EN_RXADDR, 0x01);  // Enable RX pipe 1 (unused)
    NRF24L01_WriteReg(NRF24L01_03_SETUP_AW, 0x03);   // 5 byte address width
    NRF24L01_WriteReg(NRF24L01_04_SETUP_RETR, 0x00); // no retransmits
    NRF24L01_SetBitrate(NRF24L01_BR_1M);             // 1Mbps
    NRF24L01_SetPower(RF_POWER);
    NRF24L01_Activate(0x73);                         // Activate feature register
    NRF24L01_WriteReg(NRF24L01_1C_DYNPD, 0x00);      // Disable dynamic payload length on all pipes
    NRF24L01_WriteReg(NRF24L01_1D_FEATURE, 0x01);
    NRF24L01_Activate(0x73);

    // todo: txid/rf channels relationship
}
