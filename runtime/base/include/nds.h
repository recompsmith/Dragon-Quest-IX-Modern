/* The DS as the rest of the runtime sees it: memory, I/O registers, time,
 * interrupts, the two CPUs and the services other parts provide. Hardware
 * layout follows GBATEK (DS Memory Map, DS I/O Map, DS Interrupts). */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

/* ---- running inside a host program (hosting.c) ---- */
typedef struct {
    const char* const* env;
    void (*yield)(void);
    void (*exit_console)(int code);
    const uint8_t* rom;
    size_t rom_size;
} CoreHost;
int rt_hosted(void);
char* rt_getenv(const char* name);
_Noreturn void rt_exit(int code);
int rt_yield(void);
int rt_host_rom(const uint8_t** rom, size_t* size);
#define getenv(name) rt_getenv(name)

/* ---- log ---- */
extern int g_log_level;
void rt_log(int level, const char* fmt, ...);
#define LOG_WARN(...)  rt_log(1, __VA_ARGS__)
#define LOG_INFO(...)  rt_log(2, __VA_ARGS__)
#define LOG_DEBUG(...) rt_log(3, __VA_ARGS__)
_Noreturn void rt_fatal(const char* fmt, ...);

/* ---- cartridge ---- */
extern uint8_t* g_rom;
extern size_t g_rom_size;
static inline int rom_is_eur(void) {
    return g_rom && g_rom_size >= 0x10 && !memcmp(g_rom + 0x0C, "YDQP", 4);
}

/* ---- memory ---- */
#define MAIN_RAM_SIZE (4u << 20)
#define ITCM_SIZE 0x8000u
#define DTCM_SIZE 0x4000u
#define SWRAM_SIZE 0x8000u
#define WRAM7_SIZE 0x10000u
extern uint8_t g_main_ram[MAIN_RAM_SIZE], g_itcm[ITCM_SIZE], g_dtcm[DTCM_SIZE],
               g_swram[SWRAM_SIZE], g_wram7[WRAM7_SIZE], g_palette[0x800], g_oam[0x800],
               g_vram[0xA4000];
extern uint32_t g_dtcm_base;
extern int g_itcm_enabled, g_dtcm_enabled;
void vram_remap(void);
uint8_t* vram_ptr(uint32_t addr);
uint8_t* vram7_ptr(uint32_t addr);
uint8_t* vram_bank(int b);
uint32_t vram_bank_size(int b);
uint8_t vram_bank_cnt(int b);
extern uint8_t* g_tex_slot[4];
extern uint8_t* g_texpal_slot[6];
extern uint32_t g_overlay_area_start, g_overlay_area_end, g_overlay_gen;

/* ---- 3D (gpu3d.c) ---- */
void gpu3d_vblank(void);
void gpu3d_render_start(void);
uint32_t gx_read(uint32_t off);
void gx_write(uint32_t off, uint32_t v);
int gx_irq_line(void);

/* ---- real-time clock ---- */
int64_t rtc_base(void);
void rtc_set_base(int64_t t);
int64_t rtc_utc_offset(void);
void rtc_set_utc_offset(int64_t s);

/* ---- crash reports (report.c) ---- */
void report_write(const char* reason);
int report_zip(const char* dir, int with_saves, char* out, size_t outn);
void report_log_line(const char* line);

/* ---- sound (sound.c) ---- */
void sound_advance(uint32_t cycles);
void sound_cnt_written(int n, uint32_t old);
int sound_chan_busy(int n);
uint32_t sound_read(int16_t* dst, uint32_t frames);
uint32_t sound_buffered(void);
void sound_discard(void);
void sound_set_speed(int n);
extern int g_save_backups;

/* ---- 2D (gpu2d.c) ---- */
void gpu_render_line(int line);
void gpu_frame_start(void);
void gpu_frame_done(void);
void gpu_affine_written(int engine, int bg);
extern uint8_t g_engine_layer[2][256 * 192];
enum { LAYER_OBJ = 4, LAYER_BACKDROP, LAYER_3D };
extern uint32_t g_screen[2][256 * 192];

/* ---- ARM7 memory ---- */
uint8_t  mem7_read8(uint32_t);
uint16_t mem7_read16(uint32_t);
uint32_t mem7_read32(uint32_t);
void mem7_write8(uint32_t, uint8_t);
void mem7_write16(uint32_t, uint16_t);
void mem7_write32(uint32_t, uint32_t);

/* one CPU's view of memory */
typedef struct Bus {
    uint8_t (*r8)(uint32_t);
    uint16_t (*r16)(uint32_t);
    uint32_t (*r32)(uint32_t);
    void (*w8)(uint32_t, uint8_t);
    void (*w16)(uint32_t, uint16_t);
    void (*w32)(uint32_t, uint32_t);
    void (*call)(Cpu*, uint32_t, uint32_t);
} Bus;

/* ---- I/O registers ---- */
uint32_t io9_read(uint32_t addr, int size);
void io9_write(uint32_t addr, uint32_t val, int size);
uint32_t io7_read(uint32_t addr, int size);
void io7_write(uint32_t addr, uint32_t val, int size);
extern uint8_t g_io[0x2000];     /* ARM9 0x04000000-0x04001FFF, values as written */
extern uint8_t g_io7[0x1000];    /* ARM7 0x04000000-0x04000FFF */
static inline uint32_t IO32(uint32_t off) { uint32_t v; memcpy(&v, &g_io[off], 4); return v; }
static inline uint16_t IO16(uint32_t off) { uint16_t v; memcpy(&v, &g_io[off], 2); return v; }
static inline uint32_t IO7_32(uint32_t off) { uint32_t v; memcpy(&v, &g_io7[off], 4); return v; }
static inline uint16_t IO7_16(uint32_t off) { uint16_t v; memcpy(&v, &g_io7[off], 2); return v; }

/* ---- interrupts and DMA timings ---- */
enum {
    IRQ_VBLANK = 0, IRQ_HBLANK = 1, IRQ_VCOUNT = 2, IRQ_TIMER0 = 3, IRQ_RTC = 7, IRQ_DMA0 = 8,
    IRQ_KEYPAD = 12, IRQ_IPCSYNC = 16, IRQ_IPC_SEND_EMPTY = 17, IRQ_IPC_RECV_NOT_EMPTY = 18,
    IRQ_CARD_DONE = 19, IRQ_GXFIFO = 21, IRQ_SPI = 23, IRQ_WIFI = 24
};
enum { DMA_IMMEDIATE = 0, DMA_VBLANK = 1, DMA_HBLANK = 2, DMA_CARD = 5, DMA_GXFIFO = 7, DMA_NONE = -1 };

typedef struct Timer {
    uint16_t reload, ctl;
    uint64_t start;            /* bus cycle the counter last held a known value */
    uint64_t overflows_seen;
    uint16_t latched;          /* counter value at `start` */
} Timer;
typedef struct Dma {
    uint32_t src, dst, ctl;    /* internal (running) addresses and the control word */
    int pending;               /* start timing while enabled, DMA_NONE otherwise */
} Dma;
struct ThreadSys;
typedef struct Core {
    int id;                    /* 9 or 7 */
    const char* name;
    Cpu* cpu;
    const Bus* bus;
    uint32_t ime, ie, iflags;
    int irq_depth;             /* interrupt handlers running */
    Timer timer[4];
    Dma dma[4];
    uint8_t* io;
    struct ThreadSys* threads;
    uint32_t tick;
} Core;
extern Core g_c9, g_c7;
extern Cpu g_arm9, g_arm7;
static inline Core* core_of(Cpu* c) { return c == &g_arm7 ? &g_c7 : &g_c9; }
uint32_t core_irq_handler_slot(Core* k);
uint32_t core_irq_check_slot(Core* k);

/* ---- time ---- */
extern uint64_t g_frame;
extern uint32_t g_vcount;
extern uint64_t g_frame_limit;
void irq_raise(Core* k, int bit);
void irq_service(Cpu* c);
void time_advance_lines(uint32_t lines);
void time_poll(Core* k);
void time_spend(uint32_t cycles);
uint64_t time_cycles(void);
void cpu_time_sync(void);
uint16_t timer_read(Core* k, int n);
void timer_write(Core* k, int n, uint32_t off_in_reg, uint32_t val, int size);

/* ---- DMA, cartridge (card.c), maths ---- */
void dma_write_ctl(Core* k, int ch);
void dma_on_event(Core* k, int timing);
uint32_t card_read(Core* k, uint32_t off, int size);
void card_write(Core* k, uint32_t off, uint32_t v, int size);
uint32_t card_read_data(Core* k);
void backup_init(const char* path);
int backup_flush(void);
void backup_frame(void);
const uint8_t* backup_image(uint32_t* size);
int backup_unlock_dlc(void);
extern uint64_t g_backup_written;
int backup_slot_valid(const uint8_t* b);
void backup_store(uint32_t off, const uint8_t* p, uint32_t n);
void math_div(void);
void math_sqrt(void);

/* ---- inter-processor communication ---- */
uint16_t ipc_sync_read(Core* k);
void ipc_sync_write(Core* k, uint16_t v);
uint16_t ipc_fifocnt_read(Core* k);
void ipc_fifocnt_write(Core* k, uint16_t v);
void ipc_fifo_send(Core* k, uint32_t v);
uint32_t ipc_fifo_recv(Core* k);

/* ---- the ARM7 and CPU modes ---- */
void arm7_boot(void);
void arm7_run(void);
void arm7_yield(void);
int arm7_is_running(void);
int cpu_mode_index(uint32_t mode);
void cpu_set_mode(Cpu* c, uint32_t mode);

/* ---- dispatch ---- */
void dispatch_init(void);
void dispatch7_call(Cpu* c, uint32_t target, uint32_t from);
const char* recomp_func_name(uint32_t addr);
const char* recomp7_func_name(uint32_t addr);
void overlays_init(const char* extract_dir);
void trace_dump(FILE* f, int n);

/* ---- game threads ---- */
void threads_init(void);
void threads_after_irq(Cpu* c);
void* threads_current_coro(Core* k);
void threads_start_arm7(uint32_t entry);
extern unsigned g_thread_switches;

/* ---- BIOS ---- */
void bios7_swi(Cpu* c, uint32_t n);
void cpu7_unhandled(Cpu* c, uint32_t addr, uint32_t insn);
void bios_intr_wait(Core* k, int discard, uint32_t mask);
void bios_halt(Core* k);

/* ---- input (input.c) ---- */
void input_update(void);
uint16_t input_keyinput(void);
uint16_t input_extkeyin(void);
uint16_t input_touch_adc(int channel);

/* ---- ARM7 devices ---- */
void spi_write_cnt(uint16_t v);
uint16_t spi_read_data(void);
void spi_write_data(uint16_t v);
void firmware_init(void);
const uint8_t* firmware_image(uint32_t* size);
int firmware_load_image(const char* path);
void firmware_image_set_mac(uint8_t* img, const uint8_t mac[6]);
void firmware_load_wireless(void);
void firmware_set_ap(int on, const char* dns);
void firmware_set_mac(const uint8_t mac[6]);
void firmware_write_wireless(FILE* f);
const uint8_t* firmware_user_settings(void);
uint32_t firmware_user_offset(void);
void rtc_write_reg(uint16_t v);
uint16_t rtc_read_reg(void);
typedef struct {
    int online;
    char dns[32];
    uint8_t mac[6];
    int mac_set;
    int builtin;
} WirelessConfig;
extern WirelessConfig g_wireless;
extern int g_fw_language;

/* ---- wireless and Wi-Fi Connection (wifi.c, wfc.c, net.c) ---- */
void wifi_reset(const uint8_t* firmware);
void net_deliver(const uint8_t* frame, int len);
void wfc_enable(int on, const char* dir);
int wfc_enabled(void);
void wfc_frame(void);
void wfc_poll(void);
int wfc_input(const uint8_t* frame, int len);
void net_enable(int on);
void net_seal(int on);
void wifi_set_power(int on);
void wifi_advance(uint32_t cycles);
uint16_t wifi_read16(uint32_t addr);
void wifi_write16(uint32_t addr, uint16_t val);
