/*
 * Gamma level definitions.
 *
 * Copyright (c) 2011 Samsung Electronics
 * Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _T55149GD030J_H
#define _T55149GD030J_H

/*--------------------------------------------------------Display configuration of registers----------------------------------------------------------*/
#define DEVICE_ID_REG       0x0000
#define DRIVER_OUT          0x0001
#define LCD_DRIVE           0x0002
#define ENTRY_MODE          0x0003
#define OUT_SHARP_CTRL      0x0006
#define DISP_CONT1          0x0007
#define DISP_CONT2          0x0008
#define LOW_POWER_CONT2     0x000B
#define EX_DISP_INT1        0x000C
#define EX_DISP_INT2        0x000F
#define PNL_INT_CONT1       0x0010
#define PNL_INT_CONT2       0x0011
#define PNL_INT_CONT3       0x0012
#define PNL_INT_CONT4       0x0020
#define PNL_INT_CONT5       0x0021
#define PNL_INT_CONT6       0x0022
#define POW_CONT1           0x0100
#define POW_CONT2           0x0101
#define POW_CONT3           0x0102
#define POW_CONT4           0x0103
#define POW_CONT6           0x0110
#define POW_SEQ_CONT1       0x0112
#define RAM_H_ADD_SET       0x0200
#define RAM_V_ADD_SET       0x0201
#define RAM_WRITE_DATA      0x0202
#define WND_H_1             0x0210
#define WND_H_2             0x0211
#define WND_V_1             0x0212
#define WND_V_2             0x0213
#define VCOM_H_VLT          0x0281
#define GAMM_CONT1          0x0300
#define GAMM_CONT2          0x0301
#define GAMM_CONT3          0x0302
#define GAMM_CONT4          0x0303
#define GAMM_CONT5          0x0304
#define GAMM_CONT6          0x0305
#define GAMM_CONT7          0x0306
#define GAMM_CONT8          0x0307
#define GAMM_CONT9          0x0308
#define GAMM_CONT10         0x0309
#define GAMM_CONT11         0x030A
#define GAMM_CONT12         0x030B
#define GAMM_CONT13         0x030C
#define GAMM_CONT14         0x030D
#define BASE_IMAGE_NB_LINE  0x0400
#define BASE_IM_DISP_CONT   0x0401
#define BASE_IM_V_SCR_CONT  0x0404
#define SOFT_RESET          0x0600

/*-------------------------------------------------------- Switching On Default Values----------------------------------------------------------*/
#define V_BASE_IMAGE_NB_LINE    0x3500
#define V_DRIVER_OUT            0x0100
#define V_LCD_DRIVE             0x0100
//#define V_ENTRY_MODE            0x1020 // high power consumption - correct direction - correct color
//#define V_ENTRY_MODE            0x1220 // low power consumption - correct direction - correct color
#define V_ENTRY_MODE            0x1230 // Manufacurer's recommended setting
#define V_DISP_CONT2            0x0808
#define V_LOW_POWER_CONT2       0x0010
#define V_EX_DISP_INT1          0x0110
#define V_EX_DISP_INT2          0x0000
#define V_PNL_INT_CONT1         0x0010
#define V_PNL_INT_CONT2         0x0000
#define V_PNL_INT_CONT3         0x0300
#define V_PNL_INT_CONT4         0x0110
#define V_PNL_INT_CONT5         0x0000
#define V_PNL_INT_CONT6         0x0000
#define V_WND_H_1               0x0000
#define V_WND_H_2               0x00EF
#define V_WND_V_1               0x0000
#define V_WND_V_2               0x01AF

//GAMMA CONTROL Positive Value
//P0KP1,0
#define V_GAMM_CONT1        0x0706

//P0KP3/2
#define V_GAMM_CONT2        0x0607

//P0KP5/4
#define V_GAMM_CONT3        0x0301

//P0FP1/0
#define V_GAMM_CONT4        0x0300

//P0FP3/2
#define V_GAMM_CONT5        0x0300

//P0RP1/0
#define V_GAMM_CONT6        0x0207

//V0RP1/0
#define V_GAMM_CONT7        0x0808


//GAMMA CONTROL Negative Value
//P0KN1/0
#define V_GAMM_CONT8        0x0706

//POKN3/2
#define V_GAMM_CONT9        0x0607

//POKN5/4
#define V_GAMM_CONT10       0x0301

//POFN1/0
#define V_GAMM_CONT11       0x0303

//POFN3/2
#define V_GAMM_CONT12       0x0202

//P0RN1/0
#define V_GAMM_CONT13       0x0207

//V0RN1/0
#define V_GAMM_CONT14       0x1F1F


#define V_BASE_IM_DISP_CONT     0x0001
#define V_BASE_IM_V_SCR_CONT    0x0000

//Display Control 1
#define V_DISP_CONT1            0x0001
#define V_DISP_CONT1_BIS        0x0021
#define V_DISP_CONT1_BIS1       0x0061
#define V_DISP_CONT1_BIS2       0x0173
#define V_POW_SEQ_CONT1         0x0060

//Power Control 1
#define V_POW_CONT1         0x17B0
#define V_POW_CONT1_BIS     0x14B0

//Power Control 2
#define V_POW_CONT2         0x0007
#define V_POW_CONT2_BIS     0x0014
#define V_POW_CONT2_BIS1    0x0447

//Power Control 3
#define V_POW_CONT3         0x01A8
#define V_POW_CONT3_BIS     0x01AC
#define V_POW_CONT3_BIS2    0x01BE

//Power Control 4
#define V_POW_CONT4         0x2E00
#define V_POW_CONT4_BIS     0x1B00

//Power Control 6
#define V_POW_CONT6         0x0001
#define V_POW_CONT6_BIS     0x0001

//VCOM High Voltage
#define V_VCOM_H_VLT        0x0008


#define MAX_GAMMA_LEVEL		5
#define GAMMA_TABLE_COUNT	21

#define SLEEPMSEC		0x8000
#define ENDDEF			0xC000
#define	DEFMASK			0xC000


/*LCD command sequences*/
static const unsigned short SEQ_DISPLAY_ON[] = {
	0x0007, 0x0021,
	SLEEPMSEC, 1,
	0x0110, 0x0001,
	0x0100, 0x16B0,
	0x0101, 0x0117,
	0x0102, 0x01B8,
	0x0103, 0x2e00,
	0x0281, 0x0015,
	0x0007, 0x0061,
	SLEEPMSEC, 50,
	0x0007, 0x0173,
	ENDDEF, 0x0000
};

static const unsigned short SEQ_DISPLAY_OFF[] = {
	0x0007, 0x0072,
	SLEEPMSEC, 50,
	0x0007, 0x0001,
	SLEEPMSEC, 150,
	0x0007, 0x0000,
	0x0100, 0x0680,
	0x0101, 0x0667,
	0x0102, 0x01A8,
	0x0103, 0x0E00,
	SLEEPMSEC, 10,
	0x0100, 0x0600,
	ENDDEF, 0x0000
};

static const unsigned short SEQ_STAND_BY_ON[] = {
	0x1D, 0xA1,
	SLEEPMSEC, 200,
	ENDDEF, 0x0000
};

static const unsigned short SEQ_STAND_BY_OFF[] = {
	0x1D, 0xA0,
	SLEEPMSEC, 250,
	ENDDEF, 0x0000
};

static const unsigned short SEQ_SETTING[] = {
	//0x0000, 0x0000,
	//0x0000, 0x0000,
	//0x0000, 0x0000,
	//0x0000, 0x0000,
	SLEEPMSEC, 10,
	DRIVER_OUT      ,   V_DRIVER_OUT     ,
	//DRIVER_OUT      ,   0x0     ,
    LCD_DRIVE       ,   V_LCD_DRIVE      ,
    ENTRY_MODE      ,   V_ENTRY_MODE     ,
    DISP_CONT2      ,   V_DISP_CONT2     ,
    LOW_POWER_CONT2 ,   V_LOW_POWER_CONT2,
    EX_DISP_INT2    ,   V_EX_DISP_INT2  ,
    PNL_INT_CONT1   ,   V_PNL_INT_CONT1 ,
    PNL_INT_CONT2   ,   V_PNL_INT_CONT2 ,
    PNL_INT_CONT3   ,   V_PNL_INT_CONT3 ,
    PNL_INT_CONT4   ,   V_PNL_INT_CONT4 ,
    PNL_INT_CONT5   ,   V_PNL_INT_CONT5 ,
    PNL_INT_CONT6   ,   V_PNL_INT_CONT6 ,
    WND_H_1         ,   V_WND_H_1       ,
    WND_H_2         ,   V_WND_H_2       ,
    WND_V_1         ,   V_WND_V_1       ,
    WND_V_2         ,   V_WND_V_2       ,
    GAMM_CONT1      ,   V_GAMM_CONT1    ,
    GAMM_CONT2      ,   V_GAMM_CONT2    ,
    GAMM_CONT3      ,   V_GAMM_CONT3    ,
    GAMM_CONT4      ,   V_GAMM_CONT4    ,
    GAMM_CONT5      ,   V_GAMM_CONT5    ,
    GAMM_CONT6      ,   V_GAMM_CONT6    ,
    GAMM_CONT7      ,   V_GAMM_CONT7    ,
    GAMM_CONT8      ,   V_GAMM_CONT8    ,
    GAMM_CONT9      ,   V_GAMM_CONT9    ,
    GAMM_CONT10     ,   V_GAMM_CONT10   ,
    GAMM_CONT11     ,   V_GAMM_CONT11   ,
    GAMM_CONT12     ,   V_GAMM_CONT12   ,
    GAMM_CONT13     ,   V_GAMM_CONT13   ,
    GAMM_CONT14     ,   V_GAMM_CONT14   ,
    BASE_IMAGE_NB_LINE  ,   V_BASE_IMAGE_NB_LINE, 
    BASE_IM_DISP_CONT   ,   V_BASE_IM_DISP_CONT , 
    BASE_IM_V_SCR_CONT  ,   V_BASE_IM_V_SCR_CONT, 
    //LCD POWER SETUP--------------------------------------------
    DISP_CONT1      ,   V_DISP_CONT1,    
    POW_CONT6       ,   V_POW_CONT6 ,    
    0x0012          ,   V_POW_SEQ_CONT1 ,
    POW_CONT1       ,   V_POW_CONT1 ,    
    POW_CONT2       ,   V_POW_CONT2 ,    
    POW_CONT3       ,   V_POW_CONT3 ,    
    SLEEPMSEC, 2,
    POW_CONT4       ,   V_POW_CONT4  ,   
    VCOM_H_VLT      ,   V_VCOM_H_VLT ,   
    POW_CONT2       ,   V_POW_CONT2_BIS ,
    POW_CONT3       ,   V_POW_CONT3_BIS ,
    SLEEPMSEC, 150,
    //Wait 150ms
    //vLCDinitRAM();
    //Init RAM
    //DISPLAY ON---------------------------------------
    DISP_CONT1      ,   V_DISP_CONT1_BIS ,
    SLEEPMSEC, 1,
    //Wait 1ms
    POW_CONT6       ,   V_POW_CONT6_BIS  ,
    POW_CONT1       ,   V_POW_CONT1_BIS  ,
    POW_CONT2       ,   V_POW_CONT2_BIS1 ,
    POW_CONT3       ,   V_POW_CONT3_BIS2 ,
    POW_CONT4       ,   V_POW_CONT4_BIS  ,
    VCOM_H_VLT      ,   V_VCOM_H_VLT     ,
    DISP_CONT1      ,   V_DISP_CONT1_BIS1 ,
    SLEEPMSEC, 50,
    //Wait 50ms
    DISP_CONT1      ,   V_DISP_CONT1_BIS2,
    SLEEPMSEC, 1,
    0x000F, 0x0000,
    0x000C, 0x0110,
    0x0212, 0x0000, //Window Vertical start address
    0x0213, 0x018F, //Window Vertical end address
    0x0210, 0x0000, //Window Horizontal start address
    0x0211, 0x00EF, //Window Horizontal end address	
    0x0202, 0x0000,
	ENDDEF, 0x0000
};




/* gamma value: 2.2 */
static const unsigned int ams369fg06_22_250[] = {
	0x00, 0x3f, 0x2a, 0x27, 0x27, 0x1f, 0x44,
	0x00, 0x00, 0x17, 0x24, 0x26, 0x1f, 0x43,
	0x00, 0x3f, 0x2a, 0x25, 0x24, 0x1b, 0x5c,
};

static const unsigned int ams369fg06_22_200[] = {
	0x00, 0x3f, 0x28, 0x29, 0x27, 0x21, 0x3e,
	0x00, 0x00, 0x10, 0x25, 0x27, 0x20, 0x3d,
	0x00, 0x3f, 0x28, 0x27, 0x25, 0x1d, 0x53,
};

static const unsigned int ams369fg06_22_150[] = {
	0x00, 0x3f, 0x2d, 0x29, 0x28, 0x23, 0x37,
	0x00, 0x00, 0x0b, 0x25, 0x28, 0x22, 0x36,
	0x00, 0x3f, 0x2b, 0x28, 0x26, 0x1f, 0x4a,
};

static const unsigned int ams369fg06_22_100[] = {
	0x00, 0x3f, 0x30, 0x2a, 0x2b, 0x24, 0x2f,
	0x00, 0x00, 0x00, 0x25, 0x29, 0x24, 0x2e,
	0x00, 0x3f, 0x2f, 0x29, 0x29, 0x21, 0x3f,
};

static const unsigned int ams369fg06_22_50[] = {
	0x00, 0x3f, 0x3c, 0x2c, 0x2d, 0x27, 0x24,
	0x00, 0x00, 0x00, 0x22, 0x2a, 0x27, 0x23,
	0x00, 0x3f, 0x3b, 0x2c, 0x2b, 0x24, 0x31,
};

struct ams369fg06_gamma {
	unsigned int * gamma_22_table[MAX_GAMMA_LEVEL];
};

struct ams369fg06_gamma gamma_table = {
	.gamma_22_table[0] = (unsigned int *)&ams369fg06_22_50,
	.gamma_22_table[1] = (unsigned int *)&ams369fg06_22_100,
	.gamma_22_table[2] = (unsigned int *)&ams369fg06_22_150,
	.gamma_22_table[3] = (unsigned int *)&ams369fg06_22_200,
	.gamma_22_table[4] = (unsigned int *)&ams369fg06_22_250,
};

#endif
