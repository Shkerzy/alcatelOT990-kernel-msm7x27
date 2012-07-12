
/********************************************************************************
 *                                                                               *
 *                              Focaltech Systems (R)                            *
 *                                                                               *
 *                               All Rights Reserved                             *
 *                                                                                *
 *  THIS WORK CONTAINS TRADE SECRET AND PROPRIETARY INFORMATION WHICH IS          *
 *  THE PROPERTY OF MENTOR GRAPHICS CORPORATION OR ITS LICENSORS AND IS           *
 *  SUBJECT TO LICENSE TERMS.                                                   *
 *                                                                               *
 *******************************************************************************/
 
 /*******************************************************************************
 *
 * Filename:
 * ---------
 *   : I2C_PP_Std.c 
 *
 * Project:
 * --------
 *  ctpm
 *
 * Description:
 * ------------
 * upgrade the CTPM firmware by Host side.
 *   
 *   
 * Author: 
 * -------
 * Wang xinming  
 *
 * 2010-06-07
 *
 * Last changed:
 * ------------- 
 * Author:  
 *
 * Modtime:   
 *
 * Revision: 0.1
*******************************************************************************/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>#include "ft5x02_i2c.h"
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>

#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
extern void T0_Waitms (FTS_WORD ms);



/*the follow three funcions should be implemented by user*/
/**************************************************************************************/
/*
[function]: 
    callback: read data from ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[out]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/





void delay_ms(FTS_WORD  w_ms)
{
    unsigned int i;
    unsigned j;
    for (j = 0; j<w_ms; j++)
     {
        for (i = 0; i < 1000; i++)
        {
            udelay(1);
        }
     }
    
   
    //platform related, please implement this function
    //T0_Waitms(w_ms);
}
//#endif 
/*
[function]: 
    callback: write data to ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[in]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/

FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
	int ret;   
	//struct i2c_client *i2c_client;
    ret = i2c_master_send(ft5x02_client, pbt_buf, dw_lenth);
	if(ret <=  0)
{
	printk("write_interface error!\n");
	return 0;
}    
	return 1;
}
FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
	int ret;   
	//struct i2c_client *i2c_client;
    ret = i2c_master_recv(ft5x02_client, pbt_buf, dw_lenth);
	if(ret <=  0)
{
	printk(" read_interface error!\n");
	return 0;
}    
	return 1;
}

/***************************************************************************************/


/*
[function]: 
    write a value to register.
[parameters]:
    e_reg_name[in]    :register name;
    pbt_buf[in]        :the returned register value;
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL fts_register_write(FTS_BYTE e_reg_name, FTS_BYTE bt_value)
{

      unsigned char tmp[4], ecc = 0;
	int32_t rc = 0;

	memset(tmp, 0, 2);
	tmp[0] = 0xfc;
	ecc ^= 0xfc;
	tmp[1] = e_reg_name;
	ecc ^= e_reg_name;
	tmp[2] = bt_value;
	ecc ^= bt_value;
	tmp[3] = ecc;

	rc = i2c_write_interface(I2C_CTPM_ADDRESS, tmp, 4);
	if (rc != 1){
		printk("i2c_write_interface failed!\n");
		return rc;
	}
	return 0;

}


unsigned char CTPM_FW[]=
{
#include "ft5x02_firmware_upgrade.h"
};


//#if CFG_SUPPORT_FLASH_UPGRADE 
/*
[function]: 
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;    
    btPara2[in]    :parameter 2;    
    btPara3[in]    :parameter 3;    
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL cmd_write(FTS_BYTE btcmd,FTS_BYTE btPara1,FTS_BYTE btPara2,FTS_BYTE btPara3,FTS_BYTE num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
 //return ft5x02_i2c_txdata(I2C_CTPM_ADDRESS, write_cmd, num);
}

/*
[function]: 
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_write(FTS_BYTE* pbt_buf, FTS_DWRD dw_len)
{
    POINTER_CHECK(pbt_buf);
    
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
[function]: 
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/

FTS_BOOL byte_read(FTS_BYTE* pbt_buf, FTS_BYTE bt_len)
{
    POINTER_CHECK(pbt_buf);
    
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


/*
[function]: 
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);    
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/


#define    FTS_PACKET_LENGTH        128

E_UPGRADE_ERR_TYPE fts_ctpm_fw_upgrade_with_i_file(void)
{

    FTS_BYTE*     pbt_buf = FTS_NULL;
      E_UPGRADE_ERR_TYPE  ret;
      unsigned char uc_temp;
    
    
    
    //=========FW upgrade========================*/
   pbt_buf = CTPM_FW;
   /*call the upgrade function*/
   ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));

      delay_ms(1000);
	uc_temp = read_reg(0x3b);
	printk("new version is = %x\n", uc_temp);	

      return ret;
}




E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{

    FTS_BYTE reg_val[2] = {0};
    FTS_DWRD i = 0;


    FTS_DWRD packet_number;
    FTS_DWRD j;
    FTS_DWRD temp;
    FTS_DWRD lenght;
    FTS_BYTE packet_buf[FTS_PACKET_LENGTH + 6];
    FTS_BYTE  auc_i2c_write_buf[10];
    FTS_BYTE bt_ecc;
	int ret;

      msleep(300); //make sure CTP bootup normally

    /*********Step 1:Reset  CTPM ***************************/
    /*write 0xaa to register 0xfc*/
      ret = read_reg(0x3c);
	
      fts_register_write(0x3c,0xaa);
     
      delay_ms(50);
     /*write 0x55 to register 0xfc*/
	
	fts_register_write(0x3c,0x55);
     
      delay_ms(30);
 
           auc_i2c_write_buf[0] = 0x55;
		
           ret =  i2c_write_interface(I2C_CTPM_ADDRESS, auc_i2c_write_buf, 1);
               delay_ms(1);
           auc_i2c_write_buf[0] = 0xaa;
           ret =  i2c_write_interface(I2C_CTPM_ADDRESS, auc_i2c_write_buf, 1);
        
     printk("Step 2: Enter update mode. \n");
   
       delay_ms(5);
      i = 0;
    /*********Step 3:check READ-ID**************************/
    /*send the opration head*/
    reg_val[0] = 0xbb; reg_val[1] = 0xcc;

    do{
        if(i > 3)
       {        printk("step3 :error!\n");
            return ERR_READID; 
        }
       cmd_write(0x90,0x00,0x00,0x00,4);
	 delay_ms(1);
       byte_read(reg_val,2);

        i++;
        printk("Step 3: CTPM ID,ID1 = 0x %x,ID2 = 0x %x\n", reg_val[0], reg_val[1]);
    }while(reg_val[0] != 0x79 || reg_val[1] != 0x03);

     /*********Step 4:erase app*******************************/
    cmd_write(0x61,0x00,0x00,0x00,1);
    delay_ms(1500);
    printk("Step 4: erase. \n");

    /*********Step 5:write firmware(FW) to ctpm flash************/
    bt_ecc = 0;
    printk("Step 5: start upgrade. \n");
     
    dw_lenth = dw_lenth - 8;
    printk("length 0x%x. \n", dw_lenth);
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0;j<packet_number;j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        delay_ms(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              printk("upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],temp+6);    
        delay_ms(20);
    }

     //send the last six packet
     for (i = 0; i<6; i++)
     {
         temp = 0x6ffa + i;
         packet_buf[2] = (FTS_BYTE)(temp>>8);
         packet_buf[3] = (FTS_BYTE)temp;
         temp =1;
         packet_buf[4] = (FTS_BYTE)(temp>>8);
         packet_buf[5] = (FTS_BYTE)temp;
         packet_buf[6] = pbt_buf[ dw_lenth + i]; 
         bt_ecc ^= packet_buf[6];
         byte_write(&packet_buf[0],7);  
         delay_ms(20);
     }

    	/*********Step 6: read out checksum**********************/  
      cmd_write(0xcc,0x00,0x00,0x00,1);
	byte_read(reg_val,1);
      printk("Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);

    if(reg_val[0] != bt_ecc)
    {
	printk("step 6 :ecc error!\n");
        return ERR_ECC;
    }

    	/*********Step 7: reset the new FW***********************/
    cmd_write(0x07,0x00,0x00,0x00,1);

	/*********step 8 auto calibration*************************/	
	printk("step 8 : auto calibration\n");
	delay_ms(200);
	 ret = read_reg(0x3c);
	 if(ret == 1){
	 fts_register_write(0x3c,4);
	}
	delay_ms(2000);
	for(i = 0; i < 10; i++){
		 ret = read_reg(0x3c);
	 	if(ret == 1){
			printk("step 8: ok!!!!\n");
    			return ERR_OK;
	 	}
		delay_ms(200);
	}
	 return ERR_STATUS;
}




#ifdef __cplusplus
}
#endif

