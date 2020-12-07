/*
 *	KM3NeT CLB v2 Firmware
 *  ----------------------
 *
 *	Copyright 2013 KM3NeT Collaboration
 *
 *  All Rights Reserved.
 *
 *
 *    File    : wrx_proto.h
 *    Created : 30 okt. 2014
 *    Author  : Vincent van Beveren
 */

/**
 * WRX, or whiteRabbit exchange structure
 *
 * Procedure for executing a command:
 * 1) Set cmd.<parameters> (if any)
 * 2) Set cmd.code   
 * 3) Wait for cmd.code to be cleared
 * 4) Check reply (if any) via info.cmdcode (== cmd.code before setting)
 */

#ifndef WRX_PROTO_H_
#define WRX_PROTO_H_

#include <stdint.h>
#include <stdbool.h>


#define WRX_MAGIC       0xDACE
#define WRX_VERSION     0x3

#define WRX_STATUS_MAC_ADDRESS_VALID        0x1     //!< MAC address is valid
#define WRX_STATUS_LINK_UP                  0x2     //!< Link is up
#define WRX_STATUS_AUTONEG_ON               0x4     //!< Auto negotation is on

#define WRX_COMMAND_NONE                    0x00    //!< No command
#define WRX_COMMAND_AUTONEG_ON              0x10    //!< Turn autonegotation on
#define WRX_COMMAND_AUTONEG_OFF             0x11    //!< Turn autonegotation off
#define WRX_COMMAND_GET_TUNEINFO            0x12    //!< Gets the tuneword
#define WRX_COMMAND_SET_TUNEWORD            0x13    //!< Sets the tuneword
#define WRX_COMMAND_GET_SFP_VENDOR_SN       0x14    //!< Gets the serial from the SFP
#define WRX_COMMAND_SET_THRESHOLD           0x15    //!< Sets a threshold value

#define WRX_SFP_TEMP_HIGH_ALARM                         0
#define WRX_SFP_TEMP_LOW_ALARM                          2
#define WRX_SFP_TEMP_HIGH_WARN                          4
#define WRX_SFP_TEMP_LOW_WARN                           6
#define WRX_SFP_VOLTAGE_HIGH_ALARM                      8
#define WRX_SFP_VOLTAGE_LOW_ALARM                       10
#define WRX_SFP_VOLTAGE_HIGH_WARN                       12
#define WRX_SFP_VOLTAGE_LOW_WARN                        14
#define WRX_SFP_BIAS_HIGH_ALARM                         16
#define WRX_SFP_BIAS_LOW_ALARM                          18
#define WRX_SFP_BIAS_HIGH_WARN                          20
#define WRX_SFP_BIAS_LOW_WARN                           22
#define WRX_SFP_TX_POWER_HIGH_ALARM                     24
#define WRX_SFP_TX_POWER_LOW_ALARM                      26
#define WRX_SFP_TX_POWER_HIGH_WARN                      28
#define WRX_SFP_TX_POWER_LOW_WARN                       30
#define WRX_SFP_RX_POWER_HIGH_ALARM                     32
#define WRX_SFP_RX_POWER_LOW_ALARM                      34
#define WRX_SFP_RX_POWER_HIGH_WARN                      36
#define WRX_SFP_RX_POWER_LOW_WARN                       38
#define WRX_SFP_LASER_TEMP_HIGH_ALARM                   40
#define WRX_SFP_LASER_TEMP_LOW_ALARM                    42
#define WRX_SFP_LASER_TEMP_HIGH_WARN                    44
#define WRX_SFP_LASER_TEMP_LOW_WARN                     46
#define WRX_SFP_TEC_CURRENT_HIGH_ALARM                  48
#define WRX_SFP_TEC_CURRENT_LOW_ALARM                   50
#define WRX_SFP_TEC_CURRENT_HIGH_WARN                   52
#define WRX_SFP_TEC_CURRENT_LOW_WARN                    54
#define WRX_SFP_ADC_TEMPERATURE                         96
#define WRX_SFP_ADC_VCC                                 98
#define WRX_SFP_ADC_TX_BIAS                             100
#define WRX_SFP_ADC_TX_POWER                            102
#define WRX_SFP_ADC_RX_POWER                            104
#define WRX_SFP_ADC_LASER_TEMP                          106
#define WRX_SFP_ADC_TEC_CURRENT                         108


typedef enum {
    WRX_PTP_END_OF_TABLE = 0,
    WRX_PTP_FAULTY,
    WRX_PTP_DISABLED,
    WRX_PTP_LISTENING,
    WRX_PTP_PRE_MASTER,
    WRX_PTP_MASTER,
    WRX_PTP_PASSIVE,
    WRX_PTP_UNCALIBRATED,
    WRX_PTP_SLAVE,
    WRX_PTP_Uninitialized = -1
} ptpState;

typedef enum {
    WRX_WRS_PRESENT = 100,
    WRX_WRS_S_LOCK,
    WRX_WRS_M_LOCK,
    WRX_WRS_LOCKED,
    WRX_WRS_CALIBRATION,
    WRX_WRS_CALIBRATED,
    WRX_WRS_RESP_CALIB_REQ,
    WRX_WRS_WR_LINK_ON,
    WRX_WRS_CALIBRATION_0,
    WRX_WRS_CALIBRATION_1,
    WRX_WRS_CALIBRATION_2,
    WRX_WRS_CALIBRATION_3,
    WRX_WRS_CALIBRATION_4,
    WRX_WRS_CALIBRATION_5,
    WRX_WRS_CALIBRATION_6,
    WRX_WRS_CALIBRATION_7,
    WRX_WRS_CALIBRATION_8,
    WRX_WRS_ABSCAL,
    WRX_WRS_Uninitialized = -1
} wrState;

typedef enum {
    WRX_WR_UNINITIALIZED = 0,
    WRX_WR_SYNC_NSEC,
    WRX_WR_SYNC_TAI,
    WRX_WR_SYNC_PHASE,
    WRX_WR_TRACK_PHASE,
    WRX_WR_WAIT_OFFSET_STABLE,
    WRX_WR_Uninitialized = -1
} servoState;

typedef enum {
    WRX_TUNE_PROC_NONE = 0,
    WRX_TUNE_PROC_EOPTOLINK,
    WRX_TUNE_PROC_OESOLUTIONS,
    WRX_TUNE_PROC_SIMULATION
} TuneProc;

typedef struct {
    uint8_t   tuneproc;         //!< Tune procedure supported (TuneProc)
    uint8_t   reserved;         //!< Status bits
    uint16_t  laserTmpWl;       //!< Laser temp/wavelength (Addr A2, 106-107)
    int32_t   tuneword;         //!< Current tuneword
} WrxTuneInfo;

typedef struct
{
    volatile uint32_t   status;         //!< Various status bits.
    
    volatile uint8_t    macAddr[6];     //!< WhiteRabbit MAC address
    volatile uint16_t   brdTemp;        //!< Board temperature
    
    volatile uint16_t   brdTempFrac;    //!< Fractional part of temperature
    volatile uint8_t    sfpTemp;        //!< SFP temperature
    volatile uint8_t    sfpTempFrac;    //!< Fractional part of SFP temperature
    
    volatile uint16_t   rxInputPower;   //!< Measured RX input power
    volatile uint16_t   bitslide;       //!< Bitslide value
    
    volatile int32_t    sDeltaTX;       //!< Delta TX Slave
    volatile int32_t    sDeltaRX;       //!< Delta RX Slave
    volatile int32_t    mDeltaTX;       //!< Delta TX Master
    volatile int32_t    mDeltaRX;       //!< Delta RX Master
    
    volatile int64_t    mu;             //!< Round Trip Time
    
    volatile int8_t     pState;         //! PTP State
    volatile int8_t     wState;         //! White Rabbit State
    volatile int8_t     sState;         //! White Rabbit Servo State
    
    volatile uint64_t   utcTime;        //! UTC time according to the WR PTP Core
    volatile uint16_t   txOutputPower;  //!< TX output power
    volatile uint16_t   cmdcode;        //!< Command response code
    union {
        WrxTuneInfo tuneInfo;			//!< Tuning information
        char        sfpVendorSN[16];	//!< SFP Vendor Serial Number
    } cmdreply;
} WrxInfo;


typedef struct
{
    volatile uint16_t     code;       //!<Command to execute
    union {
        volatile int32_t  tuneWord;   //!< Tune word
        struct {
            volatile uint16_t index;  //! Threshold index
            volatile uint16_t value;  //! Threshold value
        } threshold;
    } params;
} WrxCmd;


typedef struct {
    volatile uint16_t   magic;      //!< Magic number.
    volatile uint16_t   ver;        //!< Wrx client version
    volatile WrxInfo    info;       //!< WrxInfo structure
    volatile WrxCmd     cmd;        //!< WrxCmd structure
} Wrx;

#endif /* WRX_PROTO_H_ */

