#include "kernel.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_CTRL 0x376

#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT0 2
#define ATA_REG_LBA0 3
#define ATA_REG_LBA1 4
#define ATA_REG_LBA2 5
#define ATA_REG_HDDEVSEL 6
#define ATA_REG_COMMAND 7
#define ATA_REG_STATUS 7

#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF 0x20
#define ATA_SR_BSY 0x80

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_PACKET 0xA0

#define ATAPI_SIG_LBA1 0x14
#define ATAPI_SIG_LBA2 0xEB
#define ATAPI_SECTOR_SIZE 2048u

struct atapi_device {
    u16 io_base;
    u16 ctrl_base;
    u8 slave;
};

static struct atapi_device cd0_dev;
static int cd0_present = 0;

static void ata_io_wait(u16 ctrl_base) {
    (void)inb((u16)(ctrl_base + 0));
    (void)inb((u16)(ctrl_base + 0));
    (void)inb((u16)(ctrl_base + 0));
    (void)inb((u16)(ctrl_base + 0));
}

static int ata_wait_not_busy(u16 io_base, u32 timeout) {
    while (timeout-- > 0u) {
        if ((inb((u16)(io_base + ATA_REG_STATUS)) & ATA_SR_BSY) == 0u) {
            return 1;
        }
    }
    return 0;
}

static int ata_wait_drq(u16 io_base, u32 timeout) {
    while (timeout-- > 0u) {
        u8 s = inb((u16)(io_base + ATA_REG_STATUS));
        if ((s & ATA_SR_BSY) == 0u) {
            if ((s & ATA_SR_DRQ) != 0u) {
                return 1;
            }
            if ((s & (ATA_SR_ERR | ATA_SR_DF)) != 0u) {
                return 0;
            }
        }
    }
    return 0;
}

static void ata_select_drive(u16 io_base, u8 slave) {
    outb((u16)(io_base + ATA_REG_HDDEVSEL), (u8)(0xA0u | (slave ? 0x10u : 0u)));
}

static int atapi_identify(u16 io_base, u16 ctrl_base, u8 slave) {
    u8 lba1;
    u8 lba2;
    u32 i;

    ata_select_drive(io_base, slave);
    ata_io_wait(ctrl_base);

    outb((u16)(io_base + ATA_REG_SECCOUNT0), 0);
    outb((u16)(io_base + ATA_REG_LBA0), 0);
    outb((u16)(io_base + ATA_REG_LBA1), 0);
    outb((u16)(io_base + ATA_REG_LBA2), 0);
    outb((u16)(io_base + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);

    if (inb((u16)(io_base + ATA_REG_STATUS)) == 0u) {
        return 0;
    }

    if (!ata_wait_not_busy(io_base, 200000u)) {
        return 0;
    }

    lba1 = inb((u16)(io_base + ATA_REG_LBA1));
    lba2 = inb((u16)(io_base + ATA_REG_LBA2));
    if (lba1 != ATAPI_SIG_LBA1 || lba2 != ATAPI_SIG_LBA2) {
        return 0;
    }

    outb((u16)(io_base + ATA_REG_COMMAND), ATA_CMD_IDENTIFY_PACKET);
    if (!ata_wait_drq(io_base, 200000u)) {
        return 0;
    }

    for (i = 0; i < 256u; ++i) {
        (void)inw((u16)(io_base + ATA_REG_DATA));
    }
    return 1;
}

static int atapi_read_blocks(void *ctx, u32 lba, u32 count, void *out_buf) {
    struct atapi_device *dev = (struct atapi_device *)ctx;
    u8 packet[12];
    u32 block;
    u8 *dst = (u8 *)out_buf;
    u32 i;

    if (!dev || !out_buf || count == 0u) {
        return 0;
    }

    for (block = 0; block < count; ++block) {
        u32 cur_lba = lba + block;

        ata_select_drive(dev->io_base, dev->slave);
        ata_io_wait(dev->ctrl_base);

        if (!ata_wait_not_busy(dev->io_base, 200000u)) {
            return 0;
        }

        outb((u16)(dev->io_base + ATA_REG_FEATURES), 0);
        outb((u16)(dev->io_base + ATA_REG_LBA1), (u8)(ATAPI_SECTOR_SIZE & 0xFFu));
        outb((u16)(dev->io_base + ATA_REG_LBA2), (u8)((ATAPI_SECTOR_SIZE >> 8) & 0xFFu));
        outb((u16)(dev->io_base + ATA_REG_COMMAND), ATA_CMD_PACKET);

        if (!ata_wait_drq(dev->io_base, 200000u)) {
            return 0;
        }

        mem_zero(packet, sizeof(packet));
        packet[0] = 0xA8;
        packet[2] = (u8)((cur_lba >> 24) & 0xFFu);
        packet[3] = (u8)((cur_lba >> 16) & 0xFFu);
        packet[4] = (u8)((cur_lba >> 8) & 0xFFu);
        packet[5] = (u8)(cur_lba & 0xFFu);
        packet[9] = 1;

        for (i = 0; i < 6u; ++i) {
            u16 w = (u16)packet[i * 2u] | ((u16)packet[i * 2u + 1u] << 8);
            outw((u16)(dev->io_base + ATA_REG_DATA), w);
        }

        if (!ata_wait_drq(dev->io_base, 200000u)) {
            return 0;
        }

        for (i = 0; i < (ATAPI_SECTOR_SIZE / 2u); ++i) {
            u16 w = inw((u16)(dev->io_base + ATA_REG_DATA));
            dst[(block * ATAPI_SECTOR_SIZE) + (i * 2u)] = (u8)(w & 0xFFu);
            dst[(block * ATAPI_SECTOR_SIZE) + (i * 2u) + 1u] = (u8)((w >> 8) & 0xFFu);
        }

        /* Complete PACKET command before issuing the next one. Some drives
           keep BSY/DRQ transiently asserted after the data phase. */
        if (!ata_wait_not_busy(dev->io_base, 200000u)) {
            return 0;
        }
        {
            u8 s = inb((u16)(dev->io_base + ATA_REG_STATUS));
            if ((s & (ATA_SR_ERR | ATA_SR_DF)) != 0u) {
                return 0;
            }
        }
    }

    return 1;
}

int atapi_probe_and_register(void) {
    struct {
        u16 io;
        u16 ctrl;
        u8 slave;
    } candidates[] = {
        {ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 0},
        {ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 1},
        {ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 0},
        {ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 1}
    };
    u32 i;

    cd0_present = 0;

    for (i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if (atapi_identify(candidates[i].io, candidates[i].ctrl, candidates[i].slave)) {
            cd0_dev.io_base = candidates[i].io;
            cd0_dev.ctrl_base = candidates[i].ctrl;
            cd0_dev.slave = candidates[i].slave;
            cd0_present = 1;
            return storage_register_device("cd0", ATAPI_SECTOR_SIZE, atapi_read_blocks, &cd0_dev) >= 0;
        }
    }

    return 0;
}
