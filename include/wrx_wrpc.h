/*
 *	KM3NeT CLB v2 Firmware
 *  ----------------------
 *
 *	Copyright 2018 KM3NeT Collaboration
 *
 *  All Rights Reserved.
 *
 *
 *    File    : wrx_wrpc.h
 *    Created : 19 apr. 2018
 *    Author  : Vincent van Beveren, Peter Jansweijer
 */

 // Initialize WhiteRabbit Exchange 
// call at initiaization time.
void wrxInit(uint8_t mac_addr[]);

// Execute on a regular basis in main-loop
void wrxExecute();