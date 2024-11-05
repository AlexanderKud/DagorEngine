// Copyright (C) Gaijin Games KFT.  All rights reserved.
#pragma once

#define NN 0
static short hid2scan_tbl[] = {
  NN, NN, NN, NN, 0x01E, 0x030, 0x02E, 0x020,             /* 00-07 */
  0x012, 0x021, 0x022, 0x023, 0x017, 0x024, 0x025, 0x026, /* 08-0F */
  0x032, 0x031, 0x018, 0x019, 0x010, 0x013, 0x01F, 0x014, /* 10-17 */
  0x016, 0x02F, 0x011, 0x02D, 0x015, 0x02C, 0x002, 0x003, /* 18-1F */
  0x004, 0x005, 0x006, 0x007, 0x008, 0x009, 0x00A, 0x00B, /* 20-27 */
  0x01C, 0x001, 0x00E, 0x00F, 0x039, 0x00C, 0x00D, 0x01A, /* 28-2F */
  0x01B, 0x02B, 0x02B, 0x027, 0x028, 0x029, 0x033, 0x034, /* 30-37 */
  0x035, 0x03A, 0x03B, 0x03C, 0x03D, 0x03E, 0x03F, 0x040, /* 38-3F */
  0x041, 0x042, 0x043, 0x044, 0x057, 0x058, 0x0B7, 0x046, /* 40-47 */
  0x0C5, 0x0D2, 0x0C7, 0x0C9, 0x0D3, 0x0CF, 0x0D1, 0x0CD, /* 48-4F */
  0x0CB, 0x0D0, 0x0C8, 0x045, 0x0B5, 0x037, 0x04A, 0x04E, /* 50-57 */
  0x09C, 0x04F, 0x050, 0x051, 0x04B, 0x04C, 0x04D, 0x047, /* 58-5F */
  0x048, 0x049, 0x052, 0x053, 0x056, 0x0DD, NN, 0x059,    /* 60-67 */
  0x05D, 0x05E, 0x05F, NN, NN, NN, NN, NN,                /* 68-6F */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* 70-77 */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* 78-7F */
  NN, NN, NN, NN, NN, 0x07E, NN, 0x073,                   /* 80-87 */
  0x070, 0x07D, 0x079, 0x07B, 0x05C, NN, NN, NN,          /* 88-8F */
  NN, NN, 0x078, 0x077, 0x076, NN, NN, NN,                /* 90-97 */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* 98-9F */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* A0-A7 */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* A8-AF */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* B0-B7 */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* B8-BF */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* C0-C7 */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* C8-CF */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* D0-D7 */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* D8-DF */
  0x01D, 0x02A, 0x038, 0x0DB, 0x09D, 0x036, 0x0B8, 0x0DC, /* E0-E7 */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* E8-EF */
  NN, NN, NN, NN, NN, NN, NN, NN,                         /* F0-F7 */
  NN, NN, NN, NN, NN, NN, NN, NN                          /* F8-FF */
};
static short hid2scan_tbl_sz = sizeof(hid2scan_tbl) / sizeof(hid2scan_tbl[0]);

#undef NN
