// *** NB! This is generated code:
// *** awk -v VERSION=X -f lpgbt-regs.awk <lpgbt_register_map_vX.py >[this-file]
#ifndef LPGBTv0_REGS_H
#define LPGBTv0_REGS_H

#define CHIPID0_V0                       0x000
#define CHIPID1_V0                       0x001
#define CHIPID2_V0                       0x002
#define CHIPID3_V0                       0x003
#define USERID0_V0                       0x004
#define USERID1_V0                       0x005
#define USERID2_V0                       0x006
#define USERID3_V0                       0x007
#define DACCAL0_V0                       0x008
#define DACCAL1_V0                       0x009
#define DACCAL2_V0                       0x00A
#define ADCCAL0_V0                       0x00B
#define ADCCAL1_V0                       0x00C
#define ADCCAL2_V0                       0x00D
#define ADCCAL3_V0                       0x00E
#define ADCCAL4_V0                       0x00F
#define ADCCAL5_V0                       0x010
#define ADCCAL6_V0                       0x011
#define ADCCAL7_V0                       0x012
#define ADCCAL8_V0                       0x013
#define ADCCAL9_V0                       0x014
#define ADCCAL10_V0                      0x015
#define ADCCAL11_V0                      0x016
#define ADCCAL12_V0                      0x017
#define ADCCAL13_V0                      0x018
#define ADCCAL14_V0                      0x019
#define TEMPCALH_V0                      0x01A
#define TEMPCALL_V0                      0x01B
#define VREFCNTR_V0                      0x01C
#define CURDACCALH_V0                    0x01D
#define CURDACCALL_V0                    0x01E
#define EPRXLOCKFILTER_V0                0x01F
#define CLKGCONFIG0_V0                   0x020
#define CLKGCONFIG1_V0                   0x021
#define CLKGPLLRES_V0                    0x022
#define CLKGPLLINTCUR_V0                 0x023
#define CLKGPLLPROPCUR_V0                0x024
#define CLKGCDRPROPCUR_V0                0x025
#define CLKGCDRINTCUR_V0                 0x026
#define CLKGCDRFFPROPCUR_V0              0x027
#define CLKGFLLINTCUR_V0                 0x028
#define CLKGFFCAP_V0                     0x029
#define CLKGCNTOVERRIDE_V0               0x02A
#define CLKGOVERRIDECAPBANK_V0           0x02B
#define CLKGWAITTIME_V0                  0x02C
#define CLKGLFCONFIG0_V0                 0x02D
#define CLKGLFCONFIG1_V0                 0x02E
#define FAMAXHEADERFOUNDCOUNT_V0         0x02F
#define FAMAXHEADERFOUNDCOUNTAFTERNF_V0  0x030
#define FAMAXHEADERNOTFOUNDCOUNT_V0      0x031
#define FAFAMAXSKIPCYCLECOUNTAFTERNF_V0  0x032
#define PSDLLCONFIG_V0                   0x033
#define EPRXDLLCONFIG_V0                 0x034
#define FORCEENABLE_V0                   0x035
#define CHIPCONFIG_V0                    0x036
#define EQCONFIG_V0                      0x037
#define EQRES_V0                         0x038
#define LDCONFIGH_V0                     0x039
#define LDCONFIGL_V0                     0x03A
#define REFCLK_V0                        0x03B
#define SCCONFIG_V0                      0x03C
#define RESETCONFIG_V0                   0x03D
#define PGCONFIG_V0                      0x03E
#define I2CMTRANSCONFIG_V0               0x03F
#define I2CMTRANSADDRESS_V0              0x040
#define I2CMTRANSCTRL_V0                 0x041
#define I2CMTRANSDATA0_V0                0x042
#define I2CMTRANSDATA1_V0                0x043
#define I2CMTRANSDATA2_V0                0x044
#define I2CMTRANSDATA3_V0                0x045
#define I2CMTRANSDATA4_V0                0x046
#define I2CMTRANSDATA5_V0                0x047
#define I2CMTRANSDATA6_V0                0x048
#define I2CMTRANSDATA7_V0                0x049
#define I2CMTRANSDATA8_V0                0x04A
#define I2CMTRANSDATA9_V0                0x04B
#define I2CMTRANSDATA10_V0               0x04C
#define I2CMTRANSDATA11_V0               0x04D
#define I2CMTRANSDATA12_V0               0x04E
#define I2CMTRANSDATA13_V0               0x04F
#define I2CMTRANSDATA14_V0               0x050
#define I2CMTRANSDATA15_V0               0x051
#define PIODIRH_V0                       0x052
#define PIODIRL_V0                       0x053
#define PIOOUTH_V0                       0x054
#define PIOOUTL_V0                       0x055
#define PIOPULLENAH_V0                   0x056
#define PIOPULLENAL_V0                   0x057
#define PIOUPDOWNH_V0                    0x058
#define PIOUPDOWNL_V0                    0x059
#define PIODRIVESTRENGTHH_V0             0x05A
#define PIODRIVESTRENGTHL_V0             0x05B
#define PS0CONFIG_V0                     0x05C
#define PS0DELAY_V0                      0x05D
#define PS0OUTDRIVER_V0                  0x05E
#define PS1CONFIG_V0                     0x05F
#define PS1DELAY_V0                      0x060
#define PS1OUTDRIVER_V0                  0x061
#define PS2CONFIG_V0                     0x062
#define PS2DELAY_V0                      0x063
#define PS2OUTDRIVER_V0                  0x064
#define PS3CONFIG_V0                     0x065
#define PS3DELAY_V0                      0x066
#define PS3OUTDRIVER_V0                  0x067
#define DACCONFIGH_V0                    0x068
#define DACCONFIGL_V0                    0x069
#define CURDACVALUE_V0                   0x06A
#define CURDACCHN_V0                     0x06B
#define EPCLK0CHNCNTRH_V0                0x06C
#define EPCLK0CHNCNTRL_V0                0x06D
#define EPCLK1CHNCNTRH_V0                0x06E
#define EPCLK1CHNCNTRL_V0                0x06F
#define EPCLK2CHNCNTRH_V0                0x070
#define EPCLK2CHNCNTRL_V0                0x071
#define EPCLK3CHNCNTRH_V0                0x072
#define EPCLK3CHNCNTRL_V0                0x073
#define EPCLK4CHNCNTRH_V0                0x074
#define EPCLK4CHNCNTRL_V0                0x075
#define EPCLK5CHNCNTRH_V0                0x076
#define EPCLK5CHNCNTRL_V0                0x077
#define EPCLK6CHNCNTRH_V0                0x078
#define EPCLK6CHNCNTRL_V0                0x079
#define EPCLK7CHNCNTRH_V0                0x07A
#define EPCLK7CHNCNTRL_V0                0x07B
#define EPCLK8CHNCNTRH_V0                0x07C
#define EPCLK8CHNCNTRL_V0                0x07D
#define EPCLK9CHNCNTRH_V0                0x07E
#define EPCLK9CHNCNTRL_V0                0x07F
#define EPCLK10CHNCNTRH_V0               0x080
#define EPCLK10CHNCNTRL_V0               0x081
#define EPCLK11CHNCNTRH_V0               0x082
#define EPCLK11CHNCNTRL_V0               0x083
#define EPCLK12CHNCNTRH_V0               0x084
#define EPCLK12CHNCNTRL_V0               0x085
#define EPCLK13CHNCNTRH_V0               0x086
#define EPCLK13CHNCNTRL_V0               0x087
#define EPCLK14CHNCNTRH_V0               0x088
#define EPCLK14CHNCNTRL_V0               0x089
#define EPCLK15CHNCNTRH_V0               0x08A
#define EPCLK15CHNCNTRL_V0               0x08B
#define EPCLK16CHNCNTRH_V0               0x08C
#define EPCLK16CHNCNTRL_V0               0x08D
#define EPCLK17CHNCNTRH_V0               0x08E
#define EPCLK17CHNCNTRL_V0               0x08F
#define EPCLK18CHNCNTRH_V0               0x090
#define EPCLK18CHNCNTRL_V0               0x091
#define EPCLK19CHNCNTRH_V0               0x092
#define EPCLK19CHNCNTRL_V0               0x093
#define EPCLK20CHNCNTRH_V0               0x094
#define EPCLK20CHNCNTRL_V0               0x095
#define EPCLK21CHNCNTRH_V0               0x096
#define EPCLK21CHNCNTRL_V0               0x097
#define EPCLK22CHNCNTRH_V0               0x098
#define EPCLK22CHNCNTRL_V0               0x099
#define EPCLK23CHNCNTRH_V0               0x09A
#define EPCLK23CHNCNTRL_V0               0x09B
#define EPCLK24CHNCNTRH_V0               0x09C
#define EPCLK24CHNCNTRL_V0               0x09D
#define EPCLK25CHNCNTRH_V0               0x09E
#define EPCLK25CHNCNTRL_V0               0x09F
#define EPCLK26CHNCNTRH_V0               0x0A0
#define EPCLK26CHNCNTRL_V0               0x0A1
#define EPCLK27CHNCNTRH_V0               0x0A2
#define EPCLK27CHNCNTRL_V0               0x0A3
#define EPCLK28CHNCNTRH_V0               0x0A4
#define EPCLK28CHNCNTRL_V0               0x0A5
#define RESERVED1_V0                     0x0A6
#define EPTXDATARATE_V0                  0x0A7
#define EPTXCONTROL_V0                   0x0A8
#define EPTX10ENABLE_V0                  0x0A9
#define EPTX32ENABLE_V0                  0x0AA
#define EPTXECCHNCNTR_V0                 0x0AB
#define EPTX00CHNCNTR_V0                 0x0AC
#define EPTX01CHNCNTR_V0                 0x0AD
#define EPTX02CHNCNTR_V0                 0x0AE
#define EPTX03CHNCNTR_V0                 0x0AF
#define EPTX10CHNCNTR_V0                 0x0B0
#define EPTX11CHNCNTR_V0                 0x0B1
#define EPTX12CHNCNTR_V0                 0x0B2
#define EPTX13CHNCNTR_V0                 0x0B3
#define EPTX20CHNCNTR_V0                 0x0B4
#define EPTX21CHNCNTR_V0                 0x0B5
#define EPTX22CHNCNTR_V0                 0x0B6
#define EPTX23CHNCNTR_V0                 0x0B7
#define EPTX30CHNCNTR_V0                 0x0B8
#define EPTX31CHNCNTR_V0                 0x0B9
#define EPTX32CHNCNTR_V0                 0x0BA
#define EPTX33CHNCNTR_V0                 0x0BB
#define EPTX01_00CHNCNTR_V0              0x0BC
#define EPTX03_02CHNCNTR_V0              0x0BD
#define EPTX11_10CHNCNTR_V0              0x0BE
#define EPTX13_12CHNCNTR_V0              0x0BF
#define EPTX21_20CHNCNTR_V0              0x0C0
#define EPTX23_22CHNCNTR_V0              0x0C1
#define EPTX31_30CHNCNTR_V0              0x0C2
#define EPTX33_32CHNCNTR_V0              0x0C3
#define EPRX0CONTROL_V0                  0x0C4
#define EPRX1CONTROL_V0                  0x0C5
#define EPRX2CONTROL_V0                  0x0C6
#define EPRX3CONTROL_V0                  0x0C7
#define EPRX4CONTROL_V0                  0x0C8
#define EPRX5CONTROL_V0                  0x0C9
#define EPRX6CONTROL_V0                  0x0CA
#define EPRXECCONTROL_V0                 0x0CB
#define EPRX00CHNCNTR_V0                 0x0CC
#define EPRX01CHNCNTR_V0                 0x0CD
#define EPRX02CHNCNTR_V0                 0x0CE
#define EPRX03CHNCNTR_V0                 0x0CF
#define EPRX10CHNCNTR_V0                 0x0D0
#define EPRX11CHNCNTR_V0                 0x0D1
#define EPRX12CHNCNTR_V0                 0x0D2
#define EPRX13CHNCNTR_V0                 0x0D3
#define EPRX20CHNCNTR_V0                 0x0D4
#define EPRX21CHNCNTR_V0                 0x0D5
#define EPRX22CHNCNTR_V0                 0x0D6
#define EPRX23CHNCNTR_V0                 0x0D7
#define EPRX30CHNCNTR_V0                 0x0D8
#define EPRX31CHNCNTR_V0                 0x0D9
#define EPRX32CHNCNTR_V0                 0x0DA
#define EPRX33CHNCNTR_V0                 0x0DB
#define EPRX40CHNCNTR_V0                 0x0DC
#define EPRX41CHNCNTR_V0                 0x0DD
#define EPRX42CHNCNTR_V0                 0x0DE
#define EPRX43CHNCNTR_V0                 0x0DF
#define EPRX50CHNCNTR_V0                 0x0E0
#define EPRX51CHNCNTR_V0                 0x0E1
#define EPRX52CHNCNTR_V0                 0x0E2
#define EPRX53CHNCNTR_V0                 0x0E3
#define EPRX60CHNCNTR_V0                 0x0E4
#define EPRX61CHNCNTR_V0                 0x0E5
#define EPRX62CHNCNTR_V0                 0x0E6
#define EPRX63CHNCNTR_V0                 0x0E7
#define EPRXECCHNCNTR_V0                 0x0E8
#define EPRXEQ10CONTROL_V0               0x0E9
#define EPRXEQ32CONTROL_V0               0x0EA
#define EPRXEQ54CONTROL_V0               0x0EB
#define EPRXEQ6CONTROL_V0                0x0EC
#define POWERUP0_V0                      0x0ED
#define POWERUP1_V0                      0x0EE
#define POWERUP2_V0                      0x0EF
#define I2CM0CONFIG_V0                   0x0F0
#define I2CM0ADDRESS_V0                  0x0F1
#define I2CM0DATA0_V0                    0x0F2
#define I2CM0DATA1_V0                    0x0F3
#define I2CM0DATA2_V0                    0x0F4
#define I2CM0DATA3_V0                    0x0F5
#define I2CM0CMD_V0                      0x0F6
#define I2CM1CONFIG_V0                   0x0F7
#define I2CM1ADDRESS_V0                  0x0F8
#define I2CM1DATA0_V0                    0x0F9
#define I2CM1DATA1_V0                    0x0FA
#define I2CM1DATA2_V0                    0x0FB
#define I2CM1DATA3_V0                    0x0FC
#define I2CM1CMD_V0                      0x0FD
#define I2CM2CONFIG_V0                   0x0FE
#define I2CM2ADDRESS_V0                  0x0FF
#define I2CM2DATA0_V0                    0x100
#define I2CM2DATA1_V0                    0x101
#define I2CM2DATA2_V0                    0x102
#define I2CM2DATA3_V0                    0x103
#define I2CM2CMD_V0                      0x104
#define EPRXTRAIN10_V0                   0x105
#define EPRXTRAIN32_V0                   0x106
#define EPRXTRAIN54_V0                   0x107
#define EPRXTRAINEC6_V0                  0x108
#define FUSECONTROL_V0                   0x109
#define FUSEBLOWDATAA_V0                 0x10A
#define FUSEBLOWDATAB_V0                 0x10B
#define FUSEBLOWDATAC_V0                 0x10C
#define FUSEBLOWDATAD_V0                 0x10D
#define FUSEBLOWADDH_V0                  0x10E
#define FUSEBLOWADDL_V0                  0x10F
#define FUSEMAGIC_V0                     0x110
#define ADCSELECT_V0                     0x111
#define ADCMON_V0                        0x112
#define ADCCONFIG_V0                     0x113
#define EOMCONFIGH_V0                    0x114
#define EOMCONFIGL_V0                    0x115
#define EOMVOFSEL_V0                     0x116
#define PROCESSANDSEUMONITOR_V0          0x117
#define ULDATASOURCE0_V0                 0x118
#define ULDATASOURCE1_V0                 0x119
#define ULDATASOURCE2_V0                 0x11A
#define ULDATASOURCE3_V0                 0x11B
#define ULDATASOURCE4_V0                 0x11C
#define ULDATASOURCE5_V0                 0x11D
#define DPDATAPATTERN3_V0                0x11E
#define DPDATAPATTERN2_V0                0x11F
#define DPDATAPATTERN1_V0                0x120
#define DPDATAPATTERN0_V0                0x121
#define EPRXPRBS3_V0                     0x122
#define EPRXPRBS2_V0                     0x123
#define EPRXPRBS1_V0                     0x124
#define EPRXPRBS0_V0                     0x125
#define BERTSOURCE_V0                    0x126
#define BERTCONFIG_V0                    0x127
#define BERTDATAPATTERN3_V0              0x128
#define BERTDATAPATTERN2_V0              0x129
#define BERTDATAPATTERN1_V0              0x12A
#define BERTDATAPATTERN0_V0              0x12B
#define RST0_V0                          0x12C
#define RST1_V0                          0x12D
#define RST2_V0                          0x12E
#define POWERUP3_V0                      0x12F
#define POWERUP4_V0                      0x130
#define CLKTREE_V0                       0x131
#define DATAPATH_V0                      0x132
#define TO0SEL_V0                        0x133
#define TO1SEL_V0                        0x134
#define TO2SEL_V0                        0x135
#define TO3SEL_V0                        0x136
#define TO4SEL_V0                        0x137
#define TO5SEL_V0                        0x138
#define TODRIVINGSTRENGTH_V0             0x139
#define TO4DRIVER_V0                     0x13A
#define TO5DRIVER_V0                     0x13B
#define TOPREEMP_V0                      0x13C
// ### Missing address(es): 13D->13F
#define CONFIGPINS_V0                    0x140
#define I2CSLAVEADDRESS_V0               0x141
#define EPRX0LOCKED_V0                   0x142
#define EPRX0CURRENTPHASE10_V0           0x143
#define EPRX0CURRENTPHASE32_V0           0x144
#define EPRX1LOCKED_V0                   0x145
#define EPRX1CURRENTPHASE10_V0           0x146
#define EPRX1CURRENTPHASE32_V0           0x147
#define EPRX2LOCKED_V0                   0x148
#define EPRX2CURRENTPHASE10_V0           0x149
#define EPRX2CURRENTPHASE32_V0           0x14A
#define EPRX3LOCKED_V0                   0x14B
#define EPRX3CURRENTPHASE10_V0           0x14C
#define EPRX3CURRENTPHASE32_V0           0x14D
#define EPRX4LOCKED_V0                   0x14E
#define EPRX4CURRENTPHASE10_V0           0x14F
#define EPRX4CURRENTPHASE32_V0           0x150
#define EPRX5LOCKED_V0                   0x151
#define EPRX5CURRENTPHASE10_V0           0x152
#define EPRX5CURRENTPHASE32_V0           0x153
#define EPRX6LOCKED_V0                   0x154
#define EPRX6CURRENTPHASE10_V0           0x155
#define EPRX6CURRENTPHASE32_V0           0x156
#define EPRXECCURRENTPHASE_V0            0x157
#define EPRX0DLLSTATUS_V0                0x158
#define EPRX1DLLSTATUS_V0                0x159
#define EPRX2DLLSTATUS_V0                0x15A
#define EPRX3DLLSTATUS_V0                0x15B
#define EPRX4DLLSTATUS_V0                0x15C
#define EPRX5DLLSTATUS_V0                0x15D
#define EPRX6DLLSTATUS_V0                0x15E
#define I2CM0CTRL_V0                     0x15F
#define I2CM0MASK_V0                     0x160
#define I2CM0STATUS_V0                   0x161
#define I2CM0TRANCNT_V0                  0x162
#define I2CM0READBYTE_V0                 0x163
#define I2CM0READ0_V0                    0x164
#define I2CM0READ1_V0                    0x165
#define I2CM0READ2_V0                    0x166
#define I2CM0READ3_V0                    0x167
#define I2CM0READ4_V0                    0x168
#define I2CM0READ5_V0                    0x169
#define I2CM0READ6_V0                    0x16A
#define I2CM0READ7_V0                    0x16B
#define I2CM0READ8_V0                    0x16C
#define I2CM0READ9_V0                    0x16D
#define I2CM0READ10_V0                   0x16E
#define I2CM0READ11_V0                   0x16F
#define I2CM0READ12_V0                   0x170
#define I2CM0READ13_V0                   0x171
#define I2CM0READ14_V0                   0x172
#define I2CM0READ15_V0                   0x173
#define I2CM1CTRL_V0                     0x174
#define I2CM1MASK_V0                     0x175
#define I2CM1STATUS_V0                   0x176
#define I2CM1TRANCNT_V0                  0x177
#define I2CM1READBYTE_V0                 0x178
#define I2CM1READ0_V0                    0x179
#define I2CM1READ1_V0                    0x17A
#define I2CM1READ2_V0                    0x17B
#define I2CM1READ3_V0                    0x17C
#define I2CM1READ4_V0                    0x17D
#define I2CM1READ5_V0                    0x17E
#define I2CM1READ6_V0                    0x17F
#define I2CM1READ7_V0                    0x180
#define I2CM1READ8_V0                    0x181
#define I2CM1READ9_V0                    0x182
#define I2CM1READ10_V0                   0x183
#define I2CM1READ11_V0                   0x184
#define I2CM1READ12_V0                   0x185
#define I2CM1READ13_V0                   0x186
#define I2CM1READ14_V0                   0x187
#define I2CM1READ15_V0                   0x188
#define I2CM2CTRL_V0                     0x189
#define I2CM2MASK_V0                     0x18A
#define I2CM2STATUS_V0                   0x18B
#define I2CM2TRANCNT_V0                  0x18C
#define I2CM2READBYTE_V0                 0x18D
#define I2CM2READ0_V0                    0x18E
#define I2CM2READ1_V0                    0x18F
#define I2CM2READ2_V0                    0x190
#define I2CM2READ3_V0                    0x191
#define I2CM2READ4_V0                    0x192
#define I2CM2READ5_V0                    0x193
#define I2CM2READ6_V0                    0x194
#define I2CM2READ7_V0                    0x195
#define I2CM2READ8_V0                    0x196
#define I2CM2READ9_V0                    0x197
#define I2CM2READ10_V0                   0x198
#define I2CM2READ11_V0                   0x199
#define I2CM2READ12_V0                   0x19A
#define I2CM2READ13_V0                   0x19B
#define I2CM2READ14_V0                   0x19C
#define I2CM2READ15_V0                   0x19D
#define PSSTATUS_V0                      0x19E
#define PIOINH_V0                        0x19F
#define PIOINL_V0                        0x1A0
#define FUSESTATUS_V0                    0x1A1
#define FUSEVALUESA_V0                   0x1A2
#define FUSEVALUESB_V0                   0x1A3
#define FUSEVALUESC_V0                   0x1A4
#define FUSEVALUESD_V0                   0x1A5
#define PROCESSMONITORSTATUS_V0          0x1A6
#define PMFREQA_V0                       0x1A7
#define PMFREQB_V0                       0x1A8
#define PMFREQC_V0                       0x1A9
#define SEUCOUNTH_V0                     0x1AA
#define SEUCOUNTL_V0                     0x1AB
#define CLKGSTATUS0_V0                   0x1AC
#define CLKGSTATUS1_V0                   0x1AD
#define CLKGSTATUS2_V0                   0x1AE
#define CLKGSTATUS3_V0                   0x1AF
#define CLKGSTATUS4_V0                   0x1B0
#define CLKGSTATUS5_V0                   0x1B1
#define CLKGSTATUS6_V0                   0x1B2
#define CLKGSTATUS7_V0                   0x1B3
#define CLKGSTATUS8_V0                   0x1B4
#define CLKGSTATUS9_V0                   0x1B5
#define DLDPFECCORRECTIONCOUNTH_V0       0x1B6
#define DLDPFECCORRECTIONCOUNTL_V0       0x1B7
#define ADCSTATUSH_V0                    0x1B8
#define ADCSTATUSL_V0                    0x1B9
#define EOMSTATUS_V0                     0x1BA
#define EOMCOUTERVALUEH_V0               0x1BB
#define EOMCOUTERVALUEL_V0               0x1BC
#define EOMCOUNTER40MH_V0                0x1BD
#define EOMCOUNTER40ML_V0                0x1BE
#define BERTSTATUS_V0                    0x1BF
#define BERTRESULT4_V0                   0x1C0
#define BERTRESULT3_V0                   0x1C1
#define BERTRESULT2_V0                   0x1C2
#define BERTRESULT1_V0                   0x1C3
#define BERTRESULT0_V0                   0x1C4
#define ROM_V0                           0x1C5
#define PORBOR_V0                        0x1C6
#define PUSMSTATUS_V0                    0x1C7
#define PUSMACTIONS_V0                   0x1C8
#define TOVALUE_V0                       0x1C9
#define SCSTATUS_V0                      0x1CA
#define FASTATE_V0                       0x1CB
#define FACOUNTER_V0                     0x1CC
#define CONFIGERRORCOUNTERH_V0           0x1CD
#define CONFIGERRORCOUNTERL_V0           0x1CE

#endif // LPGBTv0_REGS_H