// Modified e-paper initialisation for InkBlink, derived from the board manufacturer's
// sample driver. Changes: initialisation for the JD79661 controller of hardware V1.2 and
// EPD_DisplayImageFast() (partial refresh).
// This modified version is not made, checked or supported by the board manufacturer.
// Origin and permission: see THIRD_PARTY_NOTICES.md.
#include "EPD_Init.h"


static uint8_t lut_flag = 0;
/**
   @brief       EPD busy wait
   @param       none
   @retval      none
   @note        on this board BUSY low = busy, high = idle (inverted)
*/
void EPD_READBUSY(void)
{
  while (EPD_ReadBUSY == 0)
  {

  }

}

/**
   @brief       EPD hardware and software reset
   @param       none
   @retval      none
   @note        a hardware reset is needed when leaving sleep
*/
void EPD_HW_SW_RESET(void)
{
  delay(100);
  EPD_RES_Set();
  delay(10);
  EPD_RES_Clr();
  delay(100);
  EPD_RES_Set();
  delay(100);

}

/**
   @brief       EPD enters sleep mode
   @param       none
   @retval      none
   @note        power differs between sleep modes; mode 1 (default) keeps the RAM data
*/
void EPD_Sleep(void)
{

  EPD_WR_REG(0x07);
  EPD_WR_DATA8(0xA5);
  delay(20);
}

/**
****************************************************************************
  A short description of register R22h, to make its parameters easier to understand
  R22h has the configurable bits A7 A6 A5 A4 A3 A2 A1 A0; their functions:
  A7 Enable internal clock oscillator
  A6 Turn-on DC-Dc boost
  A5 Read the temperature sensor
  A4 Search and load the LUT from OTP
  A3 Perform 0=DISPLAY Mode 1 1=DISPLAY Mode 2
  A2 Perform image display (content in RAM)sequence
  A1 Turn-off DC-DC boost
  A0 Disable the internal clock oscillator
  Temperature sensor: the EPD loads the LUT that matches its built-in temperature sensor, which is how
  different display modes (such as the fast mode in this project) work: a given temperature value selects the refresh
  Display modes: mode 1 and mode 2 can be read as full and partial refresh
****************************************************************************
*/

/**
   @brief       EPD display update, full refresh
   @param       none
   @retval      none
*/
void EPD_Update(void)
{
  EPD_WR_REG(0x17);       /* DISPLAY REFRESH */
  EPD_WR_DATA8(0xA5);
  EPD_READBUSY();
}

//Fast refresh 1 update function
void EPD_Update_Fast(void)
{   
  EPD_WR_REG(0x22); //Display Update Control
  EPD_WR_DATA8(0xC7);   
  EPD_WR_REG(0x20); //Activate Display Update Sequence
  EPD_READBUSY();   
}

/**
   @brief       EPD display update, partial refresh
   @param       none
   @retval      none
*/
void EPD_PartUpdate(void)
{
  EPD_WR_REG(0x22);  /* display mode control command, see the SSD1680 datasheet */
  /* if the clock and DC-DC were already on before the partial refresh and not turned off, they need not be turned on again; otherwise they must be! */
  /* clock on, DC-DC on, read the temperature, load the LUT, partial mode, refresh the image, keep DC-DC and the clock on*/
  EPD_WR_DATA8(0xFC); /* turned on by default here for safety; if they are already on, this can be changed to 0x1C*/
  EPD_WR_REG(0x20);
  EPD_READBUSY();

  EPD_WR_REG(0x3C);
  EPD_WR_DATA8(0x01);
}


/**
   @brief       Clear
   @param       none
   @retval      none
*/
void EPD_Clear(void)
{
    unsigned int i;

    EPD_WR_REG(0x10);       /* Transfer OLD data */
    for (i = 0; i < EPD_W * EPD_H / 8; i++)
    {
        EPD_WR_DATA8(0xFF);
    }

    EPD_WR_REG(0x13);       /* Transfer new data */
    for (i = 0; i < EPD_W * EPD_H / 8; i++)
    {
        EPD_WR_DATA8(0xFF);
    }

    lut_GC();                
}

/**
   @brief       EPD fill the whole screen with a colour
   @param       color: fill colour value
   @retval      none
*/
void EPD_ALL_Fill(uint8_t color)
{
  uint32_t i;
  EPD_WR_REG(0x3C);/* border waveform control command, see the SSD1680 datasheet */
  if (color)
  {
    EPD_WR_DATA8(0x01);
  }
  else
  {
    EPD_WR_DATA8(0x00);
  }
  EPD_WR_REG(0x24); /* write RAM command, see the SSD1680 datasheet */
  for (i = 0; i < ALLSCREEN_BYTES; i++)
  {
    EPD_WR_DATA8(color);
  }
  EPD_READBUSY();
}

/**
   @brief       EPD show an image on the whole screen
   @param       ImageBW: image array
   @retval      none
*/
void EPD_DisplayImage(const uint8_t *ImageBW)
{
  unsigned int i;

  EPD_WR_REG(0x50);
  EPD_WR_DATA8(0xD7);

  EPD_WR_REG(0x13);       /* Start data transfer */
  for (i = 0; i < EPD_W * EPD_H / 8; i++)
  {
      EPD_WR_DATA8(*ImageBW);
      ImageBW++;
  }
  lut_GC();
}

/**
   @brief       Szybkie (czesciowe) odswiezenie: wysyla STARY i NOWY obraz oraz przebieg DU.
                Potem wywolaj EPD_Update() jak zwykle. Migania nie ma, ale po kilku takich
                odswiezeniach trzeba zrobic pelne (EPD_DisplayImage + EPD_Update), zeby usunac slady.
   @param       newImg: obraz, ktory ma byc widoczny
   @param       oldImg: obraz, ktory jest widoczny teraz
   @note        EKSPERYMENTALNE: sekwencja zlozona z istniejacych polecen sterownika
                (0x10 = stare dane, 0x13 = nowe dane, lut_DU() = przebieg szybki).
*/
void EPD_DisplayImageFast(const uint8_t *newImg, const uint8_t *oldImg)
{
  unsigned int i;

  EPD_WR_REG(0x50);
  EPD_WR_DATA8(0xD7);

  EPD_WR_REG(0x10);       /* Transfer OLD data */
  for (i = 0; i < EPD_W * EPD_H / 8; i++)
  {
      EPD_WR_DATA8(oldImg[i]);
  }

  EPD_WR_REG(0x13);       /* Transfer NEW data */
  for (i = 0; i < EPD_W * EPD_H / 8; i++)
  {
      EPD_WR_DATA8(newImg[i]);
  }
  lut_DU();
}

void EPD_Init(void)
{
  EPD_GPIOInit();
  EPD_HW_SW_RESET();

    EPD_WR_REG(0x00);
    EPD_WR_DATA8(0xF7);     /* bit3:Gate scan dir  bit2:Source shift dir (0xFB/0xFF/0xF7/0xF3) */
    EPD_WR_DATA8(0x8A);     /* 0x8D */

    EPD_WR_REG(0x01);
    EPD_WR_DATA8(0x03);
    EPD_WR_DATA8(0x00);
    EPD_WR_DATA8(0x3F);
    EPD_WR_DATA8(0x3F);
    EPD_WR_DATA8(0x03);

    EPD_WR_REG(0x03);
    EPD_WR_DATA8(0x00);

    EPD_WR_REG(0x06);
    EPD_WR_DATA8(0x27);
    EPD_WR_DATA8(0x27);
    EPD_WR_DATA8(0x2F);

    EPD_WR_REG(0x30);
    EPD_WR_DATA8(0x0D);

    EPD_WR_REG(0x60);
    EPD_WR_DATA8(0x22);

    EPD_WR_REG(0x82);
    EPD_WR_DATA8(0x07);

    EPD_WR_REG(0xE3);
    EPD_WR_DATA8(0x88);

    EPD_WR_REG(0x41);
    EPD_WR_DATA8(0x00);

    EPD_WR_REG(0x61);
    EPD_WR_DATA8(0x80);
    EPD_WR_DATA8(0x00);
    EPD_WR_DATA8(0xFA);

    EPD_WR_REG(0x65);
    EPD_WR_DATA8(0x00);
    EPD_WR_DATA8(0x00);
    EPD_WR_DATA8(0x00);

    EPD_WR_REG(0x50);
    EPD_WR_DATA8(0xB7);
}

//Fast refresh 1 initialization
void EPD_HW_Init_Fast(void)
{
  EPD_RES_Clr();  // Module reset   
  delay(10);//At least 10ms delay 
  EPD_RES_Set();
  delay(10); //At least 10ms delay 
  
  EPD_WR_REG(0x12);  //SWRESET
  EPD_READBUSY();   
  
  EPD_WR_REG(0x18); //Read built-in temperature sensor
  EPD_WR_DATA8(0x80);  
      
  EPD_WR_REG(0x22); // Load temperature value
  EPD_WR_DATA8(0xB1);    
  EPD_WR_REG(0x20); 
  EPD_READBUSY();   

  EPD_WR_REG(0x1A); // Write to temperature register
  EPD_WR_DATA8(0x64);    
  EPD_WR_DATA8(0x00);  
            
  EPD_WR_REG(0x22); // Load temperature value
  EPD_WR_DATA8(0x91);    
  EPD_WR_REG(0x20); 
  EPD_READBUSY();   
}

/* ------------------------------------------------------------------ */
/*                          LUT waveform upload                              */
/* ------------------------------------------------------------------ */

/* write the GC (full refresh) waveform */
void lut_GC(void)
{
    uint8_t count;

    EPD_WR_REG(0x20);
    for (count = 0; count < 56; count++)
    {
        EPD_WR_DATA8(lut_R20_GC[count]);
    }

    EPD_WR_REG(0x21);
    for (count = 0; count < 56; count++)
    {
        EPD_WR_DATA8(lut_R21_GC[count]);
    }

    EPD_WR_REG(0x24);
    for (count = 0; count < 56; count++)
    {
        EPD_WR_DATA8(lut_R24_GC[count]);
    }

    if (lut_flag == 0)
    {
        EPD_WR_REG(0x22);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R22_GC[count]);
        }

        EPD_WR_REG(0x23);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R23_GC[count]);
        }
        lut_flag = 1;
    }
    else
    {
        /* as in the reference code: register 0x23 gets the R22 data, 0x22 gets the R23 data (alternating waveforms) */
        EPD_WR_REG(0x23);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R22_GC[count]);
        }

        EPD_WR_REG(0x22);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R23_GC[count]);
        }
        lut_flag = 0;
    }
}

/* write the DU (partial refresh) waveform */
void lut_DU(void)
{
    uint8_t count;

    EPD_WR_REG(0x20);
    for (count = 0; count < 56; count++)
    {
        EPD_WR_DATA8(lut_R20_DU[count]);
    }

    EPD_WR_REG(0x21);
    for (count = 0; count < 56; count++)
    {
        EPD_WR_DATA8(lut_R21_DU[count]);
    }

    EPD_WR_REG(0x24);
    for (count = 0; count < 56; count++)
    {
        EPD_WR_DATA8(lut_R24_DU[count]);
    }

    if (lut_flag == 0)
    {
        EPD_WR_REG(0x22);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R22_DU[count]);
        }

        EPD_WR_REG(0x23);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R23_DU[count]);
        }
        lut_flag = 1;
    }
    else
    {
        /* as in the reference code: register 0x22 gets the R23 data, 0x23 gets the R22 data (alternating waveforms) */
        EPD_WR_REG(0x22);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R23_DU[count]);
        }

        EPD_WR_REG(0x23);
        for (count = 0; count < 56; count++)
        {
            EPD_WR_DATA8(lut_R22_DU[count]);
        }
        lut_flag = 0;
    }
}

/* ================================================================== */
/*                            Waveform (LUT)                          */
/* ================================================================== */

/* ---------------------------- GC LUT (Clear) ---------------------------- */
 const unsigned char lut_R20_GC[] =  
{
  0x01,	0x00,	0x14,	0x14,	0x01,	0x00,	0x00,	0x01,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
};							  
 const unsigned char  lut_R21_GC[] =  
{
  0x01,	0x60,	0x14,	0x14,	0x01,	0x00,	0x00,	0x01,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
};						 
 const unsigned char  lut_R22_GC[] =  
{
  0x01,	0x20,	0x14,	0x14,	0x01,	0x00,	0x00,	0x01,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

};
 const unsigned char  lut_R23_GC[] =  
{
  0x01,	0x10,	0x14,	0x14,	0x01,	0x00,	0x00,	0x01,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

};
 const unsigned char  lut_R24_GC[] =  
{
  0x01,	0x90,	0x14,	0x14,	0x01,	0x00,	0x00,	0x01,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

};

// DU 300ms 
 const unsigned char lut_R20_DU[] =  
{
  0x01,	0x00,	0x14,	0x01,	0x01,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

};							  
 const unsigned char  lut_R21_DU[] =  
{
  0x01,	0x00,	0x14,	0x01,	0x01,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
};						 
 const unsigned char  lut_R22_DU[] =  
{
  0x01,	0x80,	0x14,	0x01,	0x01,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

};
 const unsigned char  lut_R23_DU[] =  
{
  0x01,	0x40,	0x14,	0x01,	0x01,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

};
 const unsigned char lut_R24_DU[] =  
{
  0x01,	0x00,	0x14,	0x01,	0x01,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
  0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,

};