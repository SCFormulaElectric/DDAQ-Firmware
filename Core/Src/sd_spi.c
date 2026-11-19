/**
 * @file    sd_spi.c
 * @brief   microSD card in SPI mode (SD v1, SDSC v2, SDHC/SDXC). Based on the
 *          standard SD SPI protocol (SD Physical Layer spec, part 1, ch. 7).
 *
 * Only called from the main loop (never from an interrupt).
 */
#include "sd_spi.h"
#include <string.h>
#include "main.h"
#include "spi.h"

/* Commands. 0x80 = application command (sent after CMD55). */
#define CMD0    (0u)          /* GO_IDLE_STATE */
#define CMD1    (1u)          /* SEND_OP_COND (MMC) */
#define ACMD41  (0x80u + 41u) /* SEND_OP_COND (SD) */
#define CMD8    (8u)          /* SEND_IF_COND */
#define CMD9    (9u)          /* SEND_CSD */
#define CMD12   (12u)         /* STOP_TRANSMISSION */
#define CMD16   (16u)         /* SET_BLOCKLEN */
#define CMD17   (17u)         /* READ_SINGLE_BLOCK */
#define CMD18   (18u)         /* READ_MULTIPLE_BLOCK */
#define ACMD23  (0x80u + 23u) /* SET_WR_BLK_ERASE_COUNT */
#define CMD24   (24u)         /* WRITE_BLOCK */
#define CMD25   (25u)         /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (55u)         /* APP_CMD */
#define CMD58   (58u)         /* READ_OCR */

/* Card type flags */
#define CT_MMC   0x01u
#define CT_SD1   0x02u
#define CT_SD2   0x04u
#define CT_BLOCK 0x08u        /* block (sector) addressing = SDHC/SDXC */

#define SECTOR_SIZE 512u

static uint8_t card_type;   /* 0 = not initialised */
static uint8_t ff_buf[SECTOR_SIZE];   /* 0xFF filler for reads, sink for writes */

/* ---- SPI helpers --------------------------------------------------------- */

static void spi_set_prescaler(uint32_t prescaler)
{
    __HAL_SPI_DISABLE(&hspi2);
    MODIFY_REG(hspi2.Instance->CR1, SPI_CR1_BR, prescaler);
    hspi2.Init.BaudRatePrescaler = prescaler;
    __HAL_SPI_ENABLE(&hspi2);
}

static uint8_t xchg(uint8_t out)
{
    uint8_t in = 0xFFu;
    HAL_SPI_TransmitReceive(&hspi2, &out, &in, 1, 10);
    return in;
}

/* Receive n bytes while sending 0xFF (the card needs MOSI high while it talks). */
static void rcvr_multi(uint8_t *buf, uint32_t n)
{
    memset(buf, 0xFF, n);
    HAL_SPI_TransmitReceive(&hspi2, buf, buf, (uint16_t)n, 100);
}

static void xmit_multi(const uint8_t *buf, uint32_t n)
{
    HAL_SPI_TransmitReceive(&hspi2, (uint8_t *)buf, ff_buf, (uint16_t)n, 100);
}

static inline void cs_high(void) { HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET); }
static inline void cs_low(void)  { HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET); }

/* Card holds MISO low while busy; 0xFF = ready. */
static uint8_t wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    do {
        if (xchg(0xFFu) == 0xFFu) return 1;
    } while ((HAL_GetTick() - start) < timeout_ms);
    return 0;
}

static void deselect(void)
{
    cs_high();
    xchg(0xFFu);     /* one extra clock so the card releases MISO */
}

static uint8_t sd_select(void)
{
    cs_low();
    xchg(0xFFu);
    if (wait_ready(500)) return 1;
    deselect();
    return 0;
}

/* ---- Command / data block primitives ------------------------------------ */

static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    if (cmd & 0x80u) {                    /* ACMDn = CMD55 + CMDn */
        cmd &= 0x7Fu;
        uint8_t r = send_cmd(CMD55, 0);
        if (r > 1u) return r;
    }

    if (cmd != CMD12) {
        deselect();
        if (!sd_select()) return 0xFFu;
    }

    uint8_t frame[6];
    frame[0] = (uint8_t)(0x40u | cmd);
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)arg;
    frame[5] = (cmd == CMD0) ? 0x95u : (cmd == CMD8) ? 0x87u : 0x01u;  /* CRC only checked for CMD0/CMD8 */
    xmit_multi(frame, sizeof frame);

    if (cmd == CMD12) xchg(0xFFu);        /* skip the stuff byte */

    uint8_t r, n = 10;
    do {
        r = xchg(0xFFu);
    } while ((r & 0x80u) && --n);
    return r;
}

static uint8_t rcvr_datablock(uint8_t *buf, uint32_t len)
{
    uint8_t token;
    uint32_t start = HAL_GetTick();
    do {
        token = xchg(0xFFu);
    } while (token == 0xFFu && (HAL_GetTick() - start) < 200u);
    if (token != 0xFEu) return 0;

    rcvr_multi(buf, len);
    xchg(0xFFu);   /* discard CRC */
    xchg(0xFFu);
    return 1;
}

static uint8_t xmit_datablock(const uint8_t *buf, uint8_t token)
{
    if (!wait_ready(500)) return 0;

    xchg(token);
    if (token == 0xFDu) return 1;         /* stop-transmission token, no data */

    xmit_multi(buf, SECTOR_SIZE);
    xchg(0xFFu);   /* dummy CRC */
    xchg(0xFFu);
    uint8_t resp = xchg(0xFFu);
    return ((resp & 0x1Fu) == 0x05u) ? 1u : 0u;   /* data accepted */
}

/* ---- Public API --------------------------------------------------------- */

void sd_spi_power(uint8_t on)
{
    /* Q7 is a P-FET: PD9 low = card powered. */
    HAL_GPIO_WritePin(SD_PWR_EN_N_GPIO_Port, SD_PWR_EN_N_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

uint8_t sd_spi_card_present(void)
{
    /* TODO(T11): read SD_CD (PD8). R141 pulls it up; check the TF PUSH socket
     * drawing (or measure with a meter) to find which level means "card in",
     * then return 1/0 accordingly. Until then we assume a card is present. */
    return 1;
}

uint8_t sd_spi_is_ready(void)
{
    return card_type != 0u;
}

sd_status_t sd_spi_init(void)
{
    uint8_t ocr[4];
    uint8_t type = 0;

    card_type = 0;
    sd_spi_power(1);
    HAL_Delay(10);                        /* Q7 soft start (~1 ms) + card power-up */

    spi_set_prescaler(SPI_BAUDRATEPRESCALER_256);   /* <= 664 kHz during init */
    cs_high();
    for (int i = 0; i < 10; i++) xchg(0xFFu);       /* >= 74 clocks with CS high */

    if (send_cmd(CMD0, 0) != 1u) {
        deselect();
        return SD_ERR_NO_RESPONSE;
    }

    uint32_t start = HAL_GetTick();
    const uint32_t init_timeout_ms = 1000u;

    if (send_cmd(CMD8, 0x1AAu) == 1u) {                       /* SD v2 */
        rcvr_multi(ocr, 4);
        if (ocr[2] == 0x01u && ocr[3] == 0xAAu) {             /* 2.7-3.6 V accepted */
            while ((HAL_GetTick() - start) < init_timeout_ms && send_cmd(ACMD41, 1UL << 30)) { }
            if ((HAL_GetTick() - start) < init_timeout_ms && send_cmd(CMD58, 0) == 0u) {
                rcvr_multi(ocr, 4);
                type = (ocr[0] & 0x40u) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
            }
        }
    } else {                                                  /* SD v1 or MMC */
        uint8_t cmd;
        if (send_cmd(ACMD41, 0) <= 1u) { type = CT_SD1; cmd = ACMD41; }
        else                           { type = CT_MMC; cmd = CMD1;   }
        while ((HAL_GetTick() - start) < init_timeout_ms && send_cmd(cmd, 0)) { }
        if ((HAL_GetTick() - start) >= init_timeout_ms || send_cmd(CMD16, SECTOR_SIZE) != 0u) {
            type = 0;
        }
    }
    deselect();

    if (type == 0u) {
        return SD_ERR_INIT;
    }
    card_type = type;
    spi_set_prescaler(SPI_BAUDRATEPRESCALER_16);    /* 10.6 MHz for data */
    return SD_OK;
}

sd_status_t sd_spi_read(uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (!card_type || count == 0u) return SD_ERR_READ;
    if (!(card_type & CT_BLOCK)) sector *= SECTOR_SIZE;    /* byte addressing on old cards */

    if (count == 1u) {
        if (send_cmd(CMD17, sector) == 0u && rcvr_datablock(buf, SECTOR_SIZE)) {
            count = 0;
        }
    } else if (send_cmd(CMD18, sector) == 0u) {
        do {
            if (!rcvr_datablock(buf, SECTOR_SIZE)) break;
            buf += SECTOR_SIZE;
        } while (--count);
        send_cmd(CMD12, 0);
    }
    deselect();
    return count ? SD_ERR_READ : SD_OK;
}

sd_status_t sd_spi_write(const uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (!card_type || count == 0u) return SD_ERR_WRITE;
    if (!(card_type & CT_BLOCK)) sector *= SECTOR_SIZE;

    if (count == 1u) {
        if (send_cmd(CMD24, sector) == 0u && xmit_datablock(buf, 0xFEu)) {
            count = 0;
        }
    } else {
        if (card_type & (CT_SD1 | CT_SD2)) send_cmd(ACMD23, count);   /* pre-erase hint */
        if (send_cmd(CMD25, sector) == 0u) {
            do {
                if (!xmit_datablock(buf, 0xFCu)) break;
                buf += SECTOR_SIZE;
            } while (--count);
            if (!xmit_datablock(NULL, 0xFDu)) count = 1;   /* stop token */
        }
    }
    deselect();
    return count ? SD_ERR_WRITE : SD_OK;
}

sd_status_t sd_spi_sync(void)
{
    sd_status_t st = sd_select() ? SD_OK : SD_ERR_TIMEOUT;
    deselect();
    return st;
}

uint32_t sd_spi_sector_count(void)
{
    uint8_t csd[16];
    uint32_t sectors = 0;

    if (send_cmd(CMD9, 0) == 0u && rcvr_datablock(csd, 16)) {
        if ((csd[0] >> 6) == 1u) {        /* CSD v2.0 (SDHC/SDXC) */
            uint32_t c_size = ((uint32_t)(csd[7] & 0x3Fu) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
            sectors = (c_size + 1u) << 10;
        } else {                          /* CSD v1.0 (SDSC, MMC) */
            uint32_t read_bl_len = csd[5] & 0x0Fu;
            uint32_t c_size = ((uint32_t)(csd[6] & 0x03u) << 10) | ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
            uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03u) << 1) | (csd[10] >> 7);
            sectors = (c_size + 1u) << (c_size_mult + 2u + read_bl_len - 9u);
        }
    }
    deselect();
    return sectors;
}
