/* original version by megazig */
#include "crypto.h"

void setup_aeskeyX(u8 keyslot, void* keyx)
{
    u32 * _keyx = (u32*)keyx;
    *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot| 0x80;
    *REG_AESKEYXFIFO = _keyx[0];
    *REG_AESKEYXFIFO = _keyx[1];
    *REG_AESKEYXFIFO = _keyx[2];
    *REG_AESKEYXFIFO = _keyx[3];
}

void setup_aeskey(u32 keyno, int value, void* key)
{
    volatile u32* aes_regs[] =
    {
        (volatile u32*)0x19009060,
        (volatile u32*)0x10009090,
        (volatile u32*)0x100090C0,
        (volatile u32*)0x100090F0
    };
    u32 * _key = (u32*)key;
    *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN|AES_CNT_INPUT_ORDER)) | (value << 23);
    if (keyno > 3)
    {
        if (keyno > 0x3F)
            return;
        *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | (u8)keyno | 0x80;
        *REG_AESKEYFIFO = _key[0];
        *REG_AESKEYFIFO = _key[1];
        *REG_AESKEYFIFO = _key[2];
        *REG_AESKEYFIFO = _key[3];
    }
    else
    {
        volatile u32* aes_reg = aes_regs[keyno];
        if (value & 0x4)
        {
            aes_reg[0] = _key[3];
            aes_reg[1] = _key[2];
            aes_reg[2] = _key[1];
            aes_reg[3] = _key[0];
        }
        else
        {
            aes_reg[0] = _key[0];
            aes_reg[1] = _key[1];
            aes_reg[2] = _key[2];
            aes_reg[3] = _key[3];
        }
    }
}

void use_aeskey(u32 keyno)
{
    if (keyno > 0x3F)
        return;
    *REG_AESKEYSEL = keyno;
    *REG_AESCNT    = *REG_AESCNT | 0x04000000; /* mystery bit */
}

void set_ctr(void* iv)
{
    u32 * _iv = (u32*)iv;
    *REG_AESCNT = (*REG_AESCNT) | AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER;
    *(REG_AESCTR + 0) = _iv[3];
    *(REG_AESCTR + 1) = _iv[2];
    *(REG_AESCTR + 2) = _iv[1];
    *(REG_AESCTR + 3) = _iv[0];
}

void add_ctr(void* ctr, u32 carry)
{
    u32 counter[4];
    u8 *outctr = (u8 *) ctr;
    u32 sum;
    int32_t i;

    for(i=0; i<4; i++) {
        counter[i] = (outctr[i*4+0]<<24) | (outctr[i*4+1]<<16) | (outctr[i*4+2]<<8) | (outctr[i*4+3]<<0);
    }

    for(i=3; i>=0; i--)
    {
        sum = counter[i] + carry;
        if (sum < counter[i]) {
            carry = 1;
        }
        else {
            carry = 0;
        }
        counter[i] = sum;
    }

    for(i=0; i<4; i++)
    {
        outctr[i*4+0] = counter[i]>>24;
        outctr[i*4+1] = counter[i]>>16;
        outctr[i*4+2] = counter[i]>>8;
        outctr[i*4+3] = counter[i]>>0;
    }
}

static void _decrypt(u32 value, void* inbuf, void* outbuf, size_t blocks)
{
    *REG_AESCNT = 0;
    *REG_AESBLKCNT = blocks << 16;
    *REG_AESCNT = value |
                  AES_CNT_START |
                  AES_CNT_FLUSH_READ |
                  AES_CNT_FLUSH_WRITE;
    aes_fifos(inbuf, outbuf, blocks);
}

void aes_decrypt(void* inbuf, void* outbuf, void* iv, size_t size, u32 mode)
{
    u32 in  = (u32)inbuf;
    u32 out = (u32)outbuf;
    size_t block_count = size;
    size_t blocks;
    while (block_count != 0)
    {
        blocks = (block_count >= 0xFFFF) ? 0xFFFF : block_count;
        _decrypt(mode, (void*)in, (void*)out, blocks);
        in  += blocks * AES_BLOCK_SIZE;
        out += blocks * AES_BLOCK_SIZE;
        block_count -= blocks;
    }
}

void aes_fifos(void* inbuf, void* outbuf, size_t blocks)
{
    u32 in  = (u32)inbuf;
    if (!in) return;

    u32 out = (u32)outbuf;
    size_t curblock = 0;
    while (curblock != blocks)
    {
        while (aescnt_checkwrite());

        int ii = 0;
        for (ii = in; ii != in + AES_BLOCK_SIZE; ii += 4)
        {
            set_aeswrfifo( *(u32*)(ii) );
        }
        if (out)
        {
            while (aescnt_checkread()) ;
            for (ii = out; ii != out + AES_BLOCK_SIZE; ii += 4)
            {
                *(u32*)ii = read_aesrdfifo();
            }
        }
        curblock++;
    }
}

void set_aeswrfifo(u32 value)
{
    *REG_AESWRFIFO = value;
}

u32 read_aesrdfifo(void)
{
    return *REG_AESRDFIFO;
}

u32 aes_getwritecount()
{
    return *REG_AESCNT & 0x1F;
}

u32 aes_getreadcount()
{
    return (*REG_AESCNT >> 5) & 0x1F;
}

u32 aescnt_checkwrite()
{
    size_t ret = aes_getwritecount();
    return (ret > 0xF);
}

u32 aescnt_checkread()
{
    size_t ret = aes_getreadcount();
    return (ret <= 3);
}


void aes_decrypt_ctx(decrypt_ctx* ctx)
{
	if(ctx->keyY != NULL)
		setup_aeskey(ctx->keyslot, AES_BIG_INPUT | AES_NORMAL_INPUT, ctx->keyY);
    use_aeskey(ctx->keyslot);

    unsigned char ctr[16] __attribute__((aligned(32)));
    memcpy(ctr, ctx->iv, 16);
	u8* buffer = ctx->buf;
	for (u32 i = 0; i < ctx->size; i += 0x10, buffer += 0x10) {
        set_ctr(ctr);
        aes_decrypt((void*)buffer, (void*)buffer, ctr, 1, ctx->mode);
        add_ctr(ctr, 0x1);
    }
}
