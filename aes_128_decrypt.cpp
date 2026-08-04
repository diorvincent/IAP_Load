/*
模块说明：用于对将要下载到电机的App .bin文件进行AES128算法解密
时间：20260803
作者：hejingchi
*/

#include "aes_128_decrypt.h"

aes_128_decrypt::aes_128_decrypt()
{

}

void aes_128_decrypt::RShiftWord(BYTE *pWord)
{
    BYTE temp = pWord[0];
    pWord[0]  = pWord[1];
    pWord[1]  = pWord[2];
    pWord[2]  = pWord[3];
    pWord[3]  = temp;
}

void aes_128_decrypt::XorBytes(BYTE *pData1, const BYTE *pData2, BYTE nCount)
{
    BYTE i;
    for (i = 0; i < nCount; i++) {
        pData1[i] ^= pData2[i];
    }
}


void aes_128_decrypt::AddKey(BYTE *pData, const BYTE *pKey)
{
    XorBytes(pData, pKey, 4 * Nb);
}


void aes_128_decrypt::SubstituteBytes(BYTE *pData, BYTE dataCnt, const BYTE *pBox)
{
    BYTE i;
    for (i = 0; i < dataCnt; i++) {
        pData[i] = pBox[pData[i]];
    }
}


void aes_128_decrypt::ShiftRows(BYTE *pState, BYTE bInvert)
{
    // 注意：状态数据以列形式存放！
    BYTE r;	// row，   行
    BYTE c;	// column，列
    BYTE temp;
    BYTE rowData[4];

    for (r = 1; r < 4; r++) {
        // 备份一行数据
        for (c = 0; c < 4; c++) {
            rowData[c] = pState[r + 4*c];
        }

        temp = bInvert ? (4 - r) : r;
        for (c = 0; c < 4; c++) {
            pState[r + 4*c] = rowData[(c + temp) % 4];
        }
    }
}


BYTE aes_128_decrypt::GfMultBy02(BYTE num)
{
    if (0 == (num & 0x80)) {
        num = num << 1;
    } else {
        num = (num << 1) ^ BPOLY;
    }

    return num;
}


void aes_128_decrypt::MixColumns(BYTE *pData, BYTE bInvert)
{
    BYTE i;
    BYTE temp;
    BYTE a0Pa2_M4;	// 4(a0 + a2)
    BYTE a1Pa3_M4;	// 4(a1 + a3)
    BYTE result[4];

    for (i = 0; i < 4; i++, pData += 4) {
        temp = pData[0] ^ pData[1] ^ pData[2] ^ pData[3];
        result[0] = temp ^ pData[0] ^ GfMultBy02((BYTE)(pData[0] ^ pData[1]));
        result[1] = temp ^ pData[1] ^ GfMultBy02((BYTE)(pData[1] ^ pData[2]));
        result[2] = temp ^ pData[2] ^ GfMultBy02((BYTE)(pData[2] ^ pData[3]));
        result[3] = temp ^ pData[3] ^ GfMultBy02((BYTE)(pData[3] ^ pData[0]));

        if (bInvert) {
            a0Pa2_M4 = GfMultBy02(GfMultBy02((BYTE)(pData[0] ^ pData[2])));
            a1Pa3_M4 = GfMultBy02(GfMultBy02((BYTE)(pData[1] ^ pData[3])));
            temp	 = GfMultBy02((BYTE)(a0Pa2_M4 ^ a1Pa3_M4));
            result[0] ^= temp ^ a0Pa2_M4;
            result[1] ^= temp ^ a1Pa3_M4;
            result[2] ^= temp ^ a0Pa2_M4;
            result[3] ^= temp ^ a1Pa3_M4;
        }

        memcpy(pData, result, 4);
    }
}


void aes_128_decrypt::BlockEncrypt(AESInfo_t *aesInfoP, BYTE *pData)
{
    BYTE i;

    AddKey(pData, aesInfoP->expandKey);
    for (i = 1; i <= aesInfoP->Nr; i++) {
        SubstituteBytes(pData, 4 * Nb, SBox);
        ShiftRows(pData, 0);

        if (i != aesInfoP->Nr) {
            MixColumns(pData, 0);
        }

        AddKey(pData, &aesInfoP->expandKey[4*Nb*i]);
    }
}


void aes_128_decrypt::BlockDecrypt(AESInfo_t *aesInfoP, BYTE *pData)
{
    BYTE i;

    AddKey(pData, &aesInfoP->expandKey[4*Nb*aesInfoP->Nr]);

    for (i = aesInfoP->Nr; i > 0; i--) {
        ShiftRows(pData, 1);
        SubstituteBytes(pData, 4 * Nb, InvSBox);
        AddKey(pData, &aesInfoP->expandKey[4*Nb*(i-1)]);

        if (1 != i) {
            MixColumns(pData, 1);
        }
    }
}


UINT aes_128_decrypt::AESAddPKCS7Padding(BYTE *data, UINT len)
{
    UINT newLen;
    newLen = len + 16 - (len % 16);
    memset(&data[len], newLen-len, newLen-len);
    return newLen;
}


UINT aes_128_decrypt::AESDelPKCS7Padding(BYTE *pData, UINT len)
{
    if (0 != (len & 0x0F)) {//1组16字节，(0 != (len & 0x0F)说明不是16的倍数
        return 0;
    }
    if (pData[len - 1] > len) {
        return 0;
    }

    return len - pData[len - 1];
}


void aes_128_decrypt::AESInit(AESInfo_t *aesInfoP)
{

    BYTE i;
    BYTE *pExpandKey;//扩展密钥
    BYTE Rcon[4] = {0x01, 0x00, 0x00, 0x00};

    switch (aesInfoP->type) {
        case AES128:
            aesInfoP->Nr = 10;
            aesInfoP->Nk = 4;
            break;
        case AES192:
            aesInfoP->Nr = 12;
            aesInfoP->Nk = 6;
            break;
        case AES256:
            aesInfoP->Nr = 14;
            aesInfoP->Nk = 8;
            break;
        default:
            aesInfoP->Nr = 10;
            aesInfoP->Nk = 4;
            break;
    }

    //拓展密匙
    memcpy(aesInfoP->expandKey, aesInfoP->key, 4 * aesInfoP->Nk);//第一个是原始密匙，
    pExpandKey = &aesInfoP->expandKey[4*aesInfoP->Nk]; //拓展密匙AES128:10个、AES192:12个、AES256:14个
    for (i = aesInfoP->Nk; i < Nb*(aesInfoP->Nr + 1); pExpandKey += 4, i++) {
        memcpy(pExpandKey, pExpandKey - 4, 4);

        if (0 == i % aesInfoP->Nk) {
            RShiftWord(pExpandKey);
            SubstituteBytes(pExpandKey, 4, SBox);
            XorBytes(pExpandKey, Rcon, 4);

            Rcon[0] = GfMultBy02(Rcon[0]);
        } else if (6 < aesInfoP->Nk && i % aesInfoP->Nk == Nb) {
            SubstituteBytes(pExpandKey, 4, SBox);
        }

        XorBytes(pExpandKey, pExpandKey - 4 * aesInfoP->Nk, 4);
    }
}


UINT aes_128_decrypt::AESEncrypt(AESInfo_t *aesInfoP, const BYTE *pPlainText,
                                         BYTE *pCipherText, UINT dataLen)
{
    UINT i;
    const void *pIV;

    if (pPlainText != pCipherText) {
        memcpy(pCipherText, pPlainText, dataLen);
    }

    //必须是16的整倍数，不够的填充，pkcs7算法是缺n补n个n，比如13字节数据缺了3个，后面就补3个3;如果刚好是16的倍数，就填充16个16
    dataLen = AESAddPKCS7Padding(pCipherText, dataLen);//注意如果是使用NOpadding方式，则此句注释掉即可，同时解密函数对应的AESDelPKCS7Padding()函数也需一同注释掉。

    pIV = aesInfoP->pIV;
    for (i = dataLen / (4 * Nb); i > 0 ; i--, pCipherText += 4 * Nb) {
        if (AES_MODE_CBC == aesInfoP->mode) {
            XorBytes(pCipherText, (const BYTE*)pIV, 4 * Nb);
        }

        BlockEncrypt(aesInfoP, pCipherText);
        pIV = pCipherText;
    }
    return dataLen;
}


UINT aes_128_decrypt::AESDecrypt(AESInfo_t *aesInfoP, BYTE *pPlainText, const BYTE *pCipherText,
                 UINT dataLen)
{
    UINT i;
    BYTE *pPlainTextBack = pPlainText;

    if (pPlainText != pCipherText) {
        memcpy(pPlainText, pCipherText, dataLen);
    }

    //当mode=AES_MODE_CBC时需要从最后一块数据开始解密
    pPlainText += dataLen - 4 * Nb;
    for (i = dataLen / (4 * Nb); i > 0 ; i--, pPlainText -= 4 * Nb) {
        BlockDecrypt(aesInfoP, pPlainText);
        if (AES_MODE_CBC == aesInfoP->mode) {
            //原来的第一块数据是初始变量加密的
            if (1 == i) {
                XorBytes(pPlainText, (const BYTE*)aesInfoP->pIV, 4 * Nb);
            } else {
                XorBytes(pPlainText, pPlainText - 4 * Nb, 4 * Nb);
            }
        }
    }

    //因为数据需要16字节对齐，可能有填充数据，需要去除后面的填充数据
    return AESDelPKCS7Padding(pPlainTextBack, dataLen);//注意如果是使用NOpadding方式，则此句注释掉直接return datalen即可，同时加密函数对应的AESAddPKCS7Padding()函数也需一同注释掉。

}

bool aes_128_decrypt::isprint(char c)
{
  bool bRight = false;
  if((c >='0' && c<='9') &&
     (c >='a' && c<='f') &&
     (c >='A' && c<= 'F'))
  {
      bRight = true;
  }
  return bRight;
}

void aes_128_decrypt::PrintData(const char *head, BYTE *data, UINT len)
{
    UINT i;

    printf("%s, len:%u:\r\n", head, len);

    //按16进制打印出来
    printf("HEX:[");
    for (i=0; i<len; i++) {
        printf("%02X ", data[i]);
    }
    printf("]\r\n");

    //按ASCII码打印出来
    printf("ASCII:[");
    for (i=0; i<len; i++) {
        if (isprint(data[i])) {//可打印字符
            printf("'%c' ", data[i]);
        } else {
            printf("\\%02X ", data[i]);
        }

    }
    printf("]\r\n");
}

void aes_128_decrypt::my_aes_init(void)
{
  //初始化
  aesInfo.type = AES128;
  aesInfo.mode = AES_MODE_CBC;
  aesInfo.key = aes_key;
  aesInfo.pIV = aes_IV;

  AESInit(&aesInfo);
}

//加密
void aes_128_decrypt::my_aes_encrypt(BYTE* sou_data, BYTE* enc_data,BYTE len)
{
    BYTE  enc_len;             //加密后的密文长度
    enc_len = AESEncrypt(&aesInfo, sou_data, enc_data, len);
    PrintData("encryptMsg", enc_data, enc_len);
}

//解密
void aes_128_decrypt::my_aes_decrypt(BYTE* enc_data, BYTE* dec_data, BYTE len)
{
    BYTE  dec_len;                //解密后的明文长度
    dec_len = AESDecrypt(&aesInfo, dec_data, enc_data, len);
    PrintData("decryptMsg", dec_data, dec_len);
}


//测试用例
void aes_128_decrypt::my_aes_test(void)
{
    //要加密的内容
    BYTE sourceMsg[8] = "hello";

    BYTE encrypt_data[33]={0};
    BYTE decrypt_data[33]={0};
    my_aes_init();
    PrintData("sourceMsg", sourceMsg, 5);
    my_aes_encrypt(sourceMsg,encrypt_data, 5);
    my_aes_decrypt(encrypt_data,decrypt_data, 16);
}
