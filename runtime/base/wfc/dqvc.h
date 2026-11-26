/* DQVC: the stock of the day at Sellma's counter (Stornway inn), made and
 * sealed on demand from the year's 52 sales. */
#pragma once
#include "wfc_ip.h"

#if defined(__ELF__) && defined(__GNUC__)
#pragma GCC visibility push(hidden)
#endif

#define DQVC_ITEMS 12                     /* items in a day's file */
#define DQVC_SALES 52

typedef struct {
    char name[24];                        /* dqvc_dNNNNN.bin */
    uint32_t number;                      /* 1000 + day */
    int64_t day;                          /* days since 2010-07-01, changing at 08:00 UTC */
    int sale;                             /* 1-52 */
    int week;                             /* weeks since the start of the sale's cycle */
    int lot_size;                         /* items of the sale */
    int count;
    uint32_t item[DQVC_ITEMS], price[DQVC_ITEMS], qty[DQVC_ITEMS];
} DqvcStock;

typedef struct {
    uint16_t item;
    uint32_t price;
    uint8_t qty;
} DqvcLot;

typedef struct {
    int16_t offset;                       /* first day, counted from the cycle's start */
    uint8_t count;
    const DqvcLot* lot;                   /* in the order of the public list */
} DqvcSale;

extern const DqvcSale dqvc_sales[DQVC_SALES];   /* dqvc_sales.c */

int64_t dqvc_day(int64_t utc);            /* DQVC day of a UTC time */
int64_t dqvc_cycle_start(int year);       /* day (since 1970) of that year's first sale */
void dqvc_today(DqvcStock* s);            /* the game's clock and this console's MAC */
void dqvc_file(const DqvcStock* s, WfcBuf* out);   /* the sealed file */

#if defined(__ELF__) && defined(__GNUC__)
#pragma GCC visibility pop
#endif
