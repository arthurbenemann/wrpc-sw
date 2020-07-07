/* Configuration for the SPEC7: Forward 125 MHz VCXO_REFCLK at CLK input to outputs 0, 1, 2 */
{
  21,
  {
  //{0x0000, 0x08}, /* Reg 0 = status info, read only */
    {0x0001, 0x00}, /* STAT1 mask */
    {0x0002, 0x00}, /* STAT2 mask */
    {0x0003, 0x70}, /* Power Down PLL, VCO and REF, no LKEN, Enable OUT[0] */
    {0x0004, 0xf0}, /* Power Down OUT[4:3]; Enable OUT[2:1]*/
    {0x0005, 0x98}, /* LKWIN = 30ns; LKCT = 128 cycles; cp = 4mA */
    {0x0006, 0x00}, /* No ChargePump intervention */
    {0x0007, 0x00}, /* No REST_R = 0 */
    {0x0008, 0x02}, /* R divider = 2 */
    {0x0009, 0x00}, /* No REST_N = 0 */
    {0x000A, 0x19}, /* N divider = 25 */
    {0x000B, 0x41}, /* SYNCMD = StandAlone; No FILTV/R */
    {0x000C, 0x80}, /* set SYNC_EN0; DEL0=0 */
    {0x000D, 0x81}, /* set IBIAS0; output divider M0 = 1 */
    {0x000E, 0x80}, /* set SYNC_EN1; DEL1=0 */
    {0x000F, 0x81}, /* set IBIAS1; output divider M1 = 1 */
    {0x0010, 0x80}, /* set SYNC_EN2; DEL2=0 */
    {0x0011, 0x81}, /* set IBIAS2; output divider M2 = 1 */
    {0x0012, 0x00}, /* no SYNC_EN2; DEL3=0 */
    {0x0013, 0x01}, /* no IBIAS3; output divider M3 = 1 */
    {0x0014, 0x00}, /* no SYNC_EN4; DEL0=0 */
    {0x0015, 0x01}  /* no RDIVOUT; output divider M4 = 1 */
  //{0x0016, 0x65} /* Reg 16 = REVision and PARTnumber, read only => 0x65 */
  }
};
